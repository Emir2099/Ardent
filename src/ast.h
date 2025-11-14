#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "token.h"

class ASTNode {
public:
    virtual ~ASTNode() = default;
};

// Optional scroll metadata header (Prologue)
struct ScrollPrologue {
    std::string title;
    std::string version;
    std::string author;
    std::unordered_map<std::string, std::string> extras; // any additional key: value pairs
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

// Represents unary operations (e.g., not expr)
class UnaryExpression : public ASTNode {
public:
    Token op;
    std::shared_ptr<ASTNode> operand;
    UnaryExpression(Token op, std::shared_ptr<ASTNode> operand)
        : op(op), operand(operand) {}
};

// Represents an explicit cast: cast <expr> as <type>
enum class CastTarget { ToNumber, ToPhrase, ToTruth };
class CastExpression : public ASTNode {
public:
    std::shared_ptr<ASTNode> operand;
    CastTarget target;
    CastExpression(std::shared_ptr<ASTNode> operand, CastTarget target)
        : operand(std::move(operand)), target(target) {}
};

// Array literal: [expr, expr, ...]
class ArrayLiteral : public ASTNode {
public:
    std::vector<std::shared_ptr<ASTNode>> elements;
    explicit ArrayLiteral(std::vector<std::shared_ptr<ASTNode>> elements)
        : elements(std::move(elements)) {}
};

// Object literal (tome): {"key": expr, ...}
class ObjectLiteral : public ASTNode {
public:
    std::vector<std::pair<std::string, std::shared_ptr<ASTNode>>> entries;
    explicit ObjectLiteral(std::vector<std::pair<std::string, std::shared_ptr<ASTNode>>> entries)
        : entries(std::move(entries)) {}
};

// Index expression: target[index]
class IndexExpression : public ASTNode {
public:
    std::shared_ptr<ASTNode> target;
    std::shared_ptr<ASTNode> index;
    IndexExpression(std::shared_ptr<ASTNode> target, std::shared_ptr<ASTNode> index)
        : target(std::move(target)), index(std::move(index)) {}
};

// Collection mutation rites
enum class CollectionRiteType { OrderExpand, OrderRemove, TomeAmend, TomeErase };
class CollectionRite : public ASTNode {
public:
    CollectionRiteType riteType;
    std::string varName; // target variable
    // Optional key or element expression
    std::shared_ptr<ASTNode> keyExpr; // for tome amend/erase (erase uses keyExpr only), or order remove element expression
    std::shared_ptr<ASTNode> valueExpr; // for order expand (element) or tome amend (new value)
    CollectionRite(CollectionRiteType t, std::string v, std::shared_ptr<ASTNode> key, std::shared_ptr<ASTNode> val)
        : riteType(t), varName(std::move(v)), keyExpr(std::move(key)), valueExpr(std::move(val)) {}
};

// Spell definition: stores name, parameter names, and body block
class BlockStatement; // forward declaration for SpellStatement body reference
class SpellStatement : public ASTNode {
public:
    std::string spellName;
    std::vector<std::string> params;
    std::shared_ptr<BlockStatement> body;
    SpellStatement(std::string name, std::vector<std::string> params, std::shared_ptr<BlockStatement> body)
        : spellName(std::move(name)), params(std::move(params)), body(std::move(body)) {}
};

// Spell invocation: call a previously defined spell with argument expressions
class SpellInvocation : public ASTNode {
public:
    std::string spellName;
    std::vector<std::shared_ptr<ASTNode>> args;
    SpellInvocation(std::string name, std::vector<std::shared_ptr<ASTNode>> args)
        : spellName(std::move(name)), args(std::move(args)) {}
};

// Native invocation: call a registered native function with argument expressions
class NativeInvocation : public ASTNode {
public:
    std::string funcName;
    std::vector<std::shared_ptr<ASTNode>> args;
    NativeInvocation(std::string name, std::vector<std::shared_ptr<ASTNode>> args)
        : funcName(std::move(name)), args(std::move(args)) {}
};

// Return statement inside spells
class ReturnStatement : public ASTNode {
public:
    std::shared_ptr<ASTNode> expression;
    explicit ReturnStatement(std::shared_ptr<ASTNode> expr) : expression(std::move(expr)) {}
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

// Import all symbols from a scroll, optional alias for spells
class ImportAll : public ASTNode {
public:
    std::string path;
    std::string alias; // empty if none
    ImportAll(std::string path, std::string alias)
        : path(std::move(path)), alias(std::move(alias)) {}
};

// Import a selection of spells from a scroll
class ImportSelective : public ASTNode {
public:
    std::string path;
    std::vector<std::string> names; // spells to take
    ImportSelective(std::string path, std::vector<std::string> names)
        : path(std::move(path)), names(std::move(names)) {}
};

// Inline include (unfurl) another scroll's contents
class UnfurlInclude : public ASTNode {
public:
    std::string path;
    explicit UnfurlInclude(std::string path) : path(std::move(path)) {}
};

// Try/Catch/Finally construct
class TryCatch : public ASTNode {
public:
    std::shared_ptr<BlockStatement> tryBlock;
    std::string catchVar; // empty if none
    std::shared_ptr<BlockStatement> catchBlock; // nullptr if none
    std::shared_ptr<BlockStatement> finallyBlock; // nullptr if none
    TryCatch(std::shared_ptr<BlockStatement> t,
             std::string cv,
             std::shared_ptr<BlockStatement> c,
             std::shared_ptr<BlockStatement> f)
        : tryBlock(std::move(t)), catchVar(std::move(cv)), catchBlock(std::move(c)), finallyBlock(std::move(f)) {}
};

struct ForLoop : public ASTNode {
    std::shared_ptr<ASTNode> init;
    std::shared_ptr<ASTNode> condition;
    std::shared_ptr<ASTNode> increment;
    TokenType stepDirection; 
    std::shared_ptr<ASTNode> body;

    ForLoop(std::shared_ptr<ASTNode> init, std::shared_ptr<ASTNode> condition, 
            std::shared_ptr<ASTNode> increment, TokenType stepDir, std::shared_ptr<ASTNode> body)
        : init(init), condition(condition), increment(increment), stepDirection(stepDir), body(body) {}
};


// Represents a while loop
class WhileLoop : public ASTNode {
    public:
        std::shared_ptr<Expression> loopVar;
        std::shared_ptr<Expression> limit;
        std::shared_ptr<Expression> step;
        TokenType comparisonOp; // Track SURPASSETH/REMAINETH
        TokenType stepDirection;
        std::vector<std::shared_ptr<ASTNode>> body;
    
        WhileLoop(std::shared_ptr<Expression> loopVar, std::shared_ptr<Expression> limit,
                  std::shared_ptr<Expression> step, TokenType comparisonOp, TokenType stepDir,
                  std::vector<std::shared_ptr<ASTNode>> body)
            : loopVar(loopVar), limit(limit), step(step), 
              comparisonOp(comparisonOp), stepDirection(stepDir), body(body) {}
    };


// Represents a do-while loop.
class DoWhileLoop : public ASTNode {
    public:
        std::shared_ptr<Expression> loopVar;
        std::shared_ptr<BlockStatement> body;
        std::shared_ptr<ASTNode> condition;
        std::shared_ptr<Expression> update;
        TokenType stepDirection; // NEW
        DoWhileLoop(std::shared_ptr<BlockStatement> body,
                    std::shared_ptr<ASTNode> condition,
                    std::shared_ptr<Expression> update,
                    std::shared_ptr<Expression> loopVar,
                    TokenType stepDir)
            : body(body), condition(condition), update(update), loopVar(loopVar), stepDirection(stepDir) {}
    };
    
    

#endif

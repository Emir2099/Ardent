#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "token.h"
#include "types.h"

// Type annotation for AST nodes (2.2 Type Runes)
struct TypeAnnotation {
    ardent::Type declaredType;   // from :rune syntax (Unknown if absent)
    ardent::Type inferredType;   // computed by type inference pass
    bool hasRune = false;        // true if user wrote :type
    int runeLine = -1;           // line number of rune for diagnostics

    TypeAnnotation() 
        : declaredType(ardent::Type::unknown())
        , inferredType(ardent::Type::unknown()) {}
};

class ASTNode {
public:
    TypeAnnotation typeInfo;     // type annotation for this node
    int sourceLine = -1;         // source line for error reporting
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
    Expression(Token token, ardent::Type declType) : token(token) {
        typeInfo.declaredType = declType;
        typeInfo.hasRune = declType.isKnown();
    }
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
    std::vector<ardent::Type> paramTypes;  // 2.2: declared param types (Unknown if untyped)
    ardent::Type returnType;               // 2.2: declared return type (Unknown if untyped)
    std::shared_ptr<BlockStatement> body;
    SpellStatement(std::string name, std::vector<std::string> params, std::shared_ptr<BlockStatement> body)
        : spellName(std::move(name)), params(std::move(params)), returnType(ardent::Type::unknown()), body(std::move(body)) {}
    SpellStatement(std::string name, std::vector<std::string> params, 
                   std::vector<ardent::Type> ptypes, ardent::Type retType,
                   std::shared_ptr<BlockStatement> body)
        : spellName(std::move(name)), params(std::move(params)), 
          paramTypes(std::move(ptypes)), returnType(std::move(retType)), body(std::move(body)) {}
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

// Variable declaration with optional type rune (2.2)
// Syntax: Let it be known, a number named x:whole is of 5 winters.
class VariableDeclaration : public ASTNode {
public:
    std::string varName;
    std::shared_ptr<ASTNode> initializer;
    ardent::Type declaredType;  // from :rune or "a number named" etc.
    bool isMutable = true;      // future: const declarations

    VariableDeclaration(std::string name, std::shared_ptr<ASTNode> init, 
                        ardent::Type declType = ardent::Type::unknown())
        : varName(std::move(name)), initializer(std::move(init)), declaredType(std::move(declType)) {
        typeInfo.declaredType = declaredType;
        typeInfo.hasRune = declaredType.isKnown();
    }
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

// ============================================================================
// ASYNC / AWAIT NODES (2.4 Living Chronicles)
// ============================================================================

// Await expression: "Await the omen of <expr>"
// Suspends execution until the promise/task completes
class AwaitExpression : public ASTNode {
public:
    std::shared_ptr<ASTNode> expression;  // The async expression to await
    
    explicit AwaitExpression(std::shared_ptr<ASTNode> expr)
        : expression(std::move(expr)) {}
};

// Spawn a background task: "Summon task <expr>"
class SpawnExpression : public ASTNode {
public:
    std::shared_ptr<ASTNode> expression;  // The expression to run as a task
    
    explicit SpawnExpression(std::shared_ptr<ASTNode> expr)
        : expression(std::move(expr)) {}
};

// ============================================================================
// STREAM NODES (2.4 Living Chronicles)
// ============================================================================

// Open a scribe (file stream): "Let a scribe be opened upon <path> [for reading/writing]"
class ScribeDeclaration : public ASTNode {
public:
    std::string scribeName;               // Variable name for the scribe
    std::shared_ptr<ASTNode> pathExpr;    // Path expression (string)
    std::string mode;                     // "read", "write", "append", "readwrite"
    
    ScribeDeclaration(std::string name, std::shared_ptr<ASTNode> path, std::string m)
        : scribeName(std::move(name)), pathExpr(std::move(path)), mode(std::move(m)) {}
};

// Write to a stream: "Write the verse <expr> into <scribe>"
class StreamWriteStatement : public ASTNode {
public:
    std::string scribeName;               // The scribe to write to
    std::shared_ptr<ASTNode> expression;  // The content to write
    
    StreamWriteStatement(std::string name, std::shared_ptr<ASTNode> expr)
        : scribeName(std::move(name)), expression(std::move(expr)) {}
};

// Close a stream: "Close the scribe <name>"
class StreamCloseStatement : public ASTNode {
public:
    std::string scribeName;               // The scribe to close
    
    explicit StreamCloseStatement(std::string name)
        : scribeName(std::move(name)) {}
};

// Read from stream line by line: "Read from scribe <name> line by line as <var>"
class StreamReadLoop : public ASTNode {
public:
    std::string scribeName;               // The scribe to read from
    std::string lineVariable;             // Variable to hold each line
    std::shared_ptr<BlockStatement> body; // Loop body
    
    StreamReadLoop(std::string scribe, std::string lineVar, std::shared_ptr<BlockStatement> b)
        : scribeName(std::move(scribe)), lineVariable(std::move(lineVar)), body(std::move(b)) {}
};

// Read entire stream: "Read all from scribe <name> into <var>"
class StreamReadAllStatement : public ASTNode {
public:
    std::string scribeName;               // The scribe to read from
    std::string targetVariable;           // Variable to hold all content
    
    StreamReadAllStatement(std::string scribe, std::string target)
        : scribeName(std::move(scribe)), targetVariable(std::move(target)) {}
};

// Check if stream is at end: "If the scribe <name> hath ended"
class StreamEofCheck : public ASTNode {
public:
    std::string scribeName;               // The scribe to check
    
    explicit StreamEofCheck(std::string name)
        : scribeName(std::move(name)) {}
};

// ============================================================================
// COLLECTION ITERATION & OPERATIONS (3.1 The Weaving of Orders)
// ============================================================================

// For-each loop: "For each X in Y:" or "For each K, V in Y:"
class ForEachStmt : public ASTNode {
public:
    std::string iterVar;                   // Main iterator variable (element or key)
    std::string valueVar;                  // Optional value variable (for tome key,value)
    std::shared_ptr<ASTNode> collection;   // The Order or Tome to iterate
    std::shared_ptr<BlockStatement> body;  // Loop body
    bool hasTwoVars = false;               // True if "key, value in tome"
    
    ForEachStmt(std::string iter, std::string val, std::shared_ptr<ASTNode> coll,
                std::shared_ptr<BlockStatement> b, bool twoVars = false)
        : iterVar(std::move(iter)), valueVar(std::move(val)), 
          collection(std::move(coll)), body(std::move(b)), hasTwoVars(twoVars) {}
};

// Membership test: "X abideth in Y"
class ContainsExpr : public ASTNode {
public:
    std::shared_ptr<ASTNode> needle;       // The value to search for
    std::shared_ptr<ASTNode> haystack;     // The Order or Tome to search in
    
    ContainsExpr(std::shared_ptr<ASTNode> n, std::shared_ptr<ASTNode> h)
        : needle(std::move(n)), haystack(std::move(h)) {}
};

// Filtering: "X where <predicate>"
class WhereExpr : public ASTNode {
public:
    std::shared_ptr<ASTNode> source;       // The Order to filter
    std::string iterVar;                   // Iterator variable name for predicate
    std::shared_ptr<ASTNode> predicate;    // Boolean expression
    
    WhereExpr(std::shared_ptr<ASTNode> src, std::string iter, std::shared_ptr<ASTNode> pred)
        : source(std::move(src)), iterVar(std::move(iter)), predicate(std::move(pred)) {}
};

// Mapping: "X transformed as <expr>"
class TransformExpr : public ASTNode {
public:
    std::shared_ptr<ASTNode> source;       // The Order to transform
    std::string iterVar;                   // Iterator variable name for expression
    std::shared_ptr<ASTNode> transform;    // Expression to apply to each element
    
    TransformExpr(std::shared_ptr<ASTNode> src, std::string iter, std::shared_ptr<ASTNode> xform)
        : source(std::move(src)), iterVar(std::move(iter)), transform(std::move(xform)) {}
};

// Index assignment: "X[i] be Y"
class IndexAssignStmt : public ASTNode {
public:
    std::shared_ptr<ASTNode> target;       // The Order or Tome
    std::shared_ptr<ASTNode> index;        // The index expression
    std::shared_ptr<ASTNode> value;        // The new value
    
    IndexAssignStmt(std::shared_ptr<ASTNode> tgt, std::shared_ptr<ASTNode> idx, std::shared_ptr<ASTNode> val)
        : target(std::move(tgt)), index(std::move(idx)), value(std::move(val)) {}
};

// Variable reassignment: "Let X become Y" (Ardent 3.3)
class VariableAssignment : public ASTNode {
public:
    std::string varName;
    std::shared_ptr<ASTNode> value;
    
    VariableAssignment(std::string name, std::shared_ptr<ASTNode> val)
        : varName(std::move(name)), value(std::move(val)) {}
};

// Block if statement: "Should the fates decree X: ... Otherwise: ..." (Ardent 3.3)
class BlockIfStatement : public ASTNode {
public:
    std::shared_ptr<ASTNode> condition;
    std::shared_ptr<BlockStatement> thenBlock;
    std::shared_ptr<BlockStatement> elseBlock;  // nullptr if no Otherwise
    
    BlockIfStatement(std::shared_ptr<ASTNode> cond, 
                     std::shared_ptr<BlockStatement> thenBlk,
                     std::shared_ptr<BlockStatement> elseBlk = nullptr)
        : condition(std::move(cond)), thenBlock(std::move(thenBlk)), elseBlock(std::move(elseBlk)) {}
};

// Break statement: "Cease" (Ardent 3.3)
class BreakStmt : public ASTNode {
public:
    BreakStmt() = default;
};

// Continue statement: "Continue" (Ardent 3.3)
class ContinueStmt : public ASTNode {
public:
    ContinueStmt() = default;
};

// New-style while loop: "Whilst condition: ... Done" (Ardent 3.3)
// Condition-based, no implicit counter, real block body
class WhileStatement : public ASTNode {
public:
    std::shared_ptr<ASTNode> condition;
    std::shared_ptr<BlockStatement> body;
    
    WhileStatement(std::shared_ptr<ASTNode> cond, std::shared_ptr<BlockStatement> bodyBlock)
        : condition(std::move(cond)), body(std::move(bodyBlock)) {}
};

// ============================================================================
// USER INPUT NODES (3.4 The Voice of the Traveler)
// ============================================================================

// Input type kinds for runtime parsing
enum class InputTypeKind { 
    Phrase,     // Default - returns string
    Whole,      // Parse as integer
    Fraction,   // Parse as float (future)
    Truth,      // Parse as boolean
    OrderWhole, // Parse space-separated integers
    OrderPhrase // Parse space-separated strings
};

// Input expression: "heard [from the traveler]" or "asked [as type] [prompt]"
// Can appear as RHS in variable declarations or inline in expressions
class InputExpression : public ASTNode {
public:
    InputTypeKind inputType;              // Type to parse input as
    std::string prompt;                   // Optional prompt string (empty if none)
    bool hasPrompt = false;               // True if user specified a prompt
    
    InputExpression(InputTypeKind type = InputTypeKind::Phrase, std::string promptStr = "")
        : inputType(type), prompt(std::move(promptStr)), hasPrompt(!prompt.empty()) {}
};
    
    

#endif

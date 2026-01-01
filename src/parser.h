#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <memory>
#include "token.h"
#include "ast.h"
#include "arena.h"
#include "types.h"

class Parser {
private:
    std::vector<Token> tokens;
    size_t current;
    Arena* arena; // optional arena for AST node allocation

    Token peek();
    Token advance();
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& errorMessage);
    bool isAtEnd();

    std::shared_ptr<ASTNode> parseExpression();
    std::shared_ptr<ASTNode> parseOr();
    std::shared_ptr<ASTNode> parseAnd();
    std::shared_ptr<ASTNode> parseUnary();
    std::shared_ptr<ASTNode> parseCast();
    std::shared_ptr<ASTNode> parseComparison();
    std::shared_ptr<ASTNode> parseSimpleCondition(); 
    std::shared_ptr<ASTNode> parseIfStatement();
    std::shared_ptr<ASTNode> parseFunctionCall();
    std::shared_ptr<ASTNode> parseSpellDefinition();
    std::shared_ptr<ASTNode> parseSpellInvocation();
    std::shared_ptr<ASTNode> parseNativeInvocation();
    std::shared_ptr<ASTNode> parseVariableDeclaration();
    std::shared_ptr<ASTNode> parseStatement();
    std::shared_ptr<ASTNode> parseWhileLoop();
    std::shared_ptr<ASTNode> parseOperatorExpression(int precedence, std::shared_ptr<ASTNode> left);
    std::shared_ptr<ASTNode> parsePrimary();
    std::shared_ptr<ASTNode> parseArrayLiteral();
    std::shared_ptr<ASTNode> parseObjectLiteral();
    std::shared_ptr<ASTNode> parseForLoop();
    std::shared_ptr<ASTNode> parseDoWhileLoop();
    std::shared_ptr<ASTNode> parseImportStatement();
    std::shared_ptr<ASTNode> parseUnfurl();
    std::shared_ptr<ASTNode> parseTryCatch();
    // Chronicle rites helpers
    std::shared_ptr<ASTNode> parseInscribe(bool append);
    std::shared_ptr<ASTNode> parseBanish();
    
public:
    Parser(std::vector<Token> tokens);
    Parser(std::vector<Token> tokens, Arena* arena);
    std::shared_ptr<ASTNode> parse();

private:
    template <typename T, typename... Args>
    std::shared_ptr<T> node(Args&&... args) {
        if (arena) {
            void* mem = arena->alloc(sizeof(T), alignof(T));
            T* ptr = new (mem) T(std::forward<Args>(args)...);
            return std::shared_ptr<T>(ptr, [](T*){/* arena-owned, no delete */});
        }
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

#endif

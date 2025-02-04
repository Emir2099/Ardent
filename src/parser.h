#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <memory>
#include "token.h"
#include "ast.h"

class Parser {
private:
    std::vector<Token> tokens;
    size_t current;

    Token peek();
    Token advance();
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& errorMessage);
    bool isAtEnd();

    std::shared_ptr<ASTNode> parseExpression();
    std::shared_ptr<ASTNode> parseSimpleCondition(); // NEW helper for if-condition
    std::shared_ptr<ASTNode> parseIfStatement();
    std::shared_ptr<ASTNode> parseFunctionCall();
    std::shared_ptr<ASTNode> parseVariableDeclaration();
    std::shared_ptr<ASTNode> parseStatement();
    std::shared_ptr<ASTNode> parseWhileLoop();

public:
    Parser(std::vector<Token> tokens);
    std::shared_ptr<ASTNode> parse();
};

#endif

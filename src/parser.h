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
    std::shared_ptr<ASTNode> parseVariableDeclaration();
    std::shared_ptr<ASTNode> parseStatement();
    std::shared_ptr<ASTNode> parseWhileLoop();
    std::shared_ptr<ASTNode> parseOperatorExpression(int precedence, std::shared_ptr<ASTNode> left);
    std::shared_ptr<ASTNode> parsePrimary();
    std::shared_ptr<ASTNode> parseArrayLiteral();
    std::shared_ptr<ASTNode> parseObjectLiteral();
    std::shared_ptr<ASTNode> parseForLoop();
    std::shared_ptr<ASTNode> parseDoWhileLoop();
    
public:
    Parser(std::vector<Token> tokens);
    std::shared_ptr<ASTNode> parse();
};

#endif

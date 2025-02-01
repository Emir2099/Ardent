#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <memory>
#include "token.h"
#include "ast.h"

class Parser {
private:
    std::vector<Token> tokens;
    size_t current = 0;

    Token peek();
    Token advance();
    bool match(TokenType type);
    
    std::shared_ptr<ASTNode> parseExpression();
    std::shared_ptr<ASTNode> parseStatement();
    std::shared_ptr<ASTNode> parseFunctionCall();
    std::shared_ptr<ASTNode> parseIfStatement();

public:
    Parser(std::vector<Token> tokens);
    std::shared_ptr<ASTNode> parse();
};

#endif

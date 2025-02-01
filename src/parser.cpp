#include "parser.h"
#include "token.h"
#include <iostream>

Parser::Parser(std::vector<Token> tokens) : tokens(tokens), current(0) {}

Token Parser::peek() {
    if (current < tokens.size()) {
        return tokens[current];
    }
    Token invalidToken = Token(TokenType::END, "");
    return invalidToken;
}

Token Parser::advance() {
    return (current < tokens.size()) ? tokens[current++] : Token(TokenType::INVALID, "");
}

bool Parser::match(TokenType type) {
    if (peek().type == type) {
        advance();
        return true;
    }
    return false;
}

std::shared_ptr<ASTNode> Parser::parseExpression() {
    Token token = advance();
    return std::make_shared<Expression>(token);
}

std::shared_ptr<ASTNode> Parser::parseIfStatement() {
    if (match(TokenType::SHOULD)) {
        auto condition = parseExpression();
        if (match(TokenType::THEN)) {
            auto thenBranch = parseExpression();
            std::shared_ptr<ASTNode> elseBranch = nullptr;
            if (match(TokenType::ELSE)) {
                elseBranch = parseExpression();
            }
            return std::make_shared<IfStatement>(condition, thenBranch, elseBranch);
        }
    }
    return nullptr;
}

std::shared_ptr<ASTNode> Parser::parseFunctionCall() {
    if (match(TokenType::SPELL_NAMED)) {
        Token functionName = advance();
        if (match(TokenType::CAST_UPON)) {
            std::vector<std::shared_ptr<ASTNode>> args;
            while (!match(TokenType::END)) {
                args.push_back(parseExpression());
            }
            return std::make_shared<FunctionCall>(functionName.value, args);
        }
    }
    return nullptr;
}

std::shared_ptr<ASTNode> Parser::parseStatement() {
    if (peek().type == TokenType::SHOULD) {
        return parseIfStatement();
    } else if (peek().type == TokenType::SPELL_NAMED) {
        return parseFunctionCall();
    } else {
        return parseExpression();
    }
}

std::shared_ptr<ASTNode> Parser::parse() {
    return parseStatement();
}
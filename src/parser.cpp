#include "parser.h"
#include "token.h"
#include "ast.h"
#include <iostream>
#include <memory>
#include <vector>
#include <sstream>

// Constructor
Parser::Parser(std::vector<Token> tokens) : tokens(tokens), current(0) {}

// Returns the current token without consuming it.
Token Parser::peek() {
    if (current < tokens.size()) {
        return tokens[current];
    }
    return Token(TokenType::END, "");
}

// Advances and returns the current token.
Token Parser::advance() {
    return (current < tokens.size()) ? tokens[current++] : Token(TokenType::INVALID, "");
}

// If the current token matches type, consume it and return true.
bool Parser::match(TokenType type) {
    if (peek().type == type) {
        advance();
        return true;
    }
    return false;
}

// Consumes a token of the given type; if not, prints an error.
Token Parser::consume(TokenType type, const std::string& errorMessage) {
    if (peek().type == type) {
        return advance();
    }
    std::cerr << "Error: " << errorMessage << " at position " << current << std::endl;
    return Token(TokenType::INVALID, "");
}

// Returns true if we've reached the end of tokens.
bool Parser::isAtEnd() {
    return current >= tokens.size();
}

// A simple parseExpression that consumes a single token.
// Inside parser.cpp

std::shared_ptr<ASTNode> Parser::parseExpression() {
    auto left = parsePrimary(); // Parse left side (string/number/identifier)
    return parseOperatorExpression(0, std::move(left)); // Handle operators
}

std::shared_ptr<ASTNode> Parser::parsePrimary() {
    Token token = peek();
    if (token.type == TokenType::STRING || token.type == TokenType::NUMBER || token.type == TokenType::IDENTIFIER) {
        token = advance();  // Consume the token once
        return std::make_shared<Expression>(token);
    } else {
        std::cerr << "Unexpected token: " << token.value << std::endl;
        return nullptr;
    }
}

std::shared_ptr<ASTNode> Parser::parseOperatorExpression(int minPrecedence, std::shared_ptr<ASTNode> left) {
    while (true) {
        Token opToken = peek();
        if (opToken.type != TokenType::OPERATOR) break;
        advance();

        auto right = parsePrimary();
        left = std::make_shared<BinaryExpression>(left, opToken, right);
    }
    return left;
}

/* NEW: Parse a simple condition in the form:
      <variable> SURPASSETH <number>
   This function skips over filler tokens such as "the", "fates", "decree", "that". */
std::shared_ptr<ASTNode> Parser::parseSimpleCondition() {
    // Skip filler tokens (you may adjust the set as needed)
    while (!isAtEnd() && 
           (peek().type == TokenType::FATES ||
            peek().type == TokenType::DECREE ||
            (peek().type == TokenType::IDENTIFIER && 
             (peek().value == "the" || peek().value == "that"))))
    {
        advance();
    }
    // Expect the variable name.
    Token left = consume(TokenType::IDENTIFIER, "Expected variable name in condition");
    // Expect the 'surpasseth' operator.
    Token op = consume(TokenType::SURPASSETH, "Expected 'surpasseth' in condition");
    // Expect a numeric value.
    Token right = consume(TokenType::NUMBER, "Expected a numeric value in condition");
    return std::make_shared<BinaryExpression>(
        std::make_shared<Expression>(left),
        op,
        std::make_shared<Expression>(right)
    );
}

// Parse an if-statement.
// Assumes that the SHOULD token has already been matched.
std::shared_ptr<ASTNode> Parser::parseIfStatement() {
    // Use the simple condition parser to parse a binary condition.
    auto condition = parseSimpleCondition();
    if (!match(TokenType::THEN)) {
        std::cerr << "Error: Expected THEN after IF condition at position " << current << std::endl;
        return nullptr;
    }
    // For the then-branch: if the next token is LET_PROCLAIMED, treat it as a print statement.
    std::shared_ptr<ASTNode> thenBranch;
    if (match(TokenType::LET_PROCLAIMED)) {
        auto expr = parseExpression();
        thenBranch = std::make_shared<PrintStatement>(expr);
    } else {
        thenBranch = parseExpression();
    }
    std::shared_ptr<ASTNode> elseBranch = nullptr;
    if (match(TokenType::ELSE)) {
        if (match(TokenType::WHISPER)) {
            auto expr = parseExpression();
            elseBranch = std::make_shared<PrintStatement>(expr);
        } else {
            elseBranch = parseExpression();
        }
    }
    return std::make_shared<IfStatement>(condition, thenBranch, elseBranch);
}

// Stub for function calls.
std::shared_ptr<ASTNode> Parser::parseFunctionCall() {
    // Implementation for function calls goes here.
    return nullptr;
}

// Parse a variable declaration.
// After encountering LET, skip filler tokens until we find NAMED.
std::shared_ptr<ASTNode> Parser::parseVariableDeclaration() {
    while (!isAtEnd() && peek().type != TokenType::NAMED) {
        advance();
    }
    if (!match(TokenType::NAMED)) {
        std::cerr << "Error: Expected 'named' after 'Let it be known' at position " << current << std::endl;
        return nullptr;
    }
    Token varName = consume(TokenType::IDENTIFIER, "Expected variable name after 'named'");
    consume(TokenType::IS_OF, "Expected 'is of' after variable name");
    Token value = consume(TokenType::NUMBER, "Expected a numeric value after 'is of'");
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER) {
        std::string filler = peek().value;
        if (filler == "winters" || filler == "years") {
            advance();
        }
    }
    return std::make_shared<BinaryExpression>(
        std::make_shared<Expression>(varName),
        Token(TokenType::IS_OF, "is of"),
        std::make_shared<Expression>(value)
    );
}

// 🔹 Parse a while loop
std::shared_ptr<ASTNode> Parser::parseWhileLoop() {
    Token loopVar = consume(TokenType::IDENTIFIER, "Expected loop variable after 'Whilst the sun doth rise'");
    consume(TokenType::REMAINETH, "Expected 'remaineth below'");
    Token limit = consume(TokenType::NUMBER, "Expected numeric limit after 'remaineth below'");
    consume(TokenType::SPOKEN, "Expected 'so shall these words be spoken'");

    std::vector<std::shared_ptr<ASTNode>> body;
    while (!match(TokenType::ASCEND)) {  // Stop when we reach "And with each dawn..."
        auto expr = parseExpression();
        if (expr) {
            // Wrap each expression in a PrintStatement
            auto printStmt = std::make_shared<PrintStatement>(expr);
            body.push_back(printStmt);
        } else {
            std::cerr << "Error: Failed to parse expression in while loop body" << std::endl;
        }
    }

    Token step = consume(TokenType::NUMBER, "Expected increment value after 'let count ascend'");

    return std::make_shared<WhileLoop>(
        std::make_shared<Expression>(loopVar),
        std::make_shared<Expression>(limit),
        std::make_shared<Expression>(step),
        body
    );
}


std::shared_ptr<ASTNode> Parser::parseForLoop() {
    // After matching "For", parse the loop variable
    Token loopVarToken = consume(TokenType::IDENTIFIER, "Expected loop variable after 'For'");
    auto loopVar = std::make_shared<Expression>(loopVarToken);

    // Expect "remaineth below"
    consume(TokenType::REMAINETH, "Expected 'remaineth below' after loop variable");

    // Parse the limit (e.g., 5)
    auto limit = parseExpression();

    // Create condition: count < 5
    Token op(TokenType::REMAINETH, "remaineth below");
    auto condition = std::make_shared<BinaryExpression>(loopVar, op, limit);

    // Parse the loop body
    consume(TokenType::SPOKEN, "Expected 'so shall these words be spoken'");
    std::vector<std::shared_ptr<ASTNode>> body;
    while (!match(TokenType::ASCEND)) {
        auto expr = parseExpression();
        if (expr) body.push_back(std::make_shared<PrintStatement>(expr));
    }

    // Parse the increment step (e.g., 1)
    auto step = parseExpression();

    return std::make_shared<ForLoop>(loopVar, condition, step, std::make_shared<BlockStatement>(body));
}

// Parse a single statement.
std::shared_ptr<ASTNode> Parser::parseStatement() {
    if (match(TokenType::LET)) {
        return parseVariableDeclaration();
    } else if (match(TokenType::SHOULD)) {
        return parseIfStatement();
    } else if (match(TokenType::SPELL_NAMED)) {
        return parseFunctionCall();
    } else if (match(TokenType::WHILST)) {  // Detects while loop
        return parseWhileLoop();
    }  else if (match(TokenType::FOR)) {  // Handle FOR loops
        return parseForLoop();
    }   
    else if (match(TokenType::LET_PROCLAIMED)) {
        auto expr = parseExpression();
        return std::make_shared<PrintStatement>(expr);
    } else {
        std::cerr << "Error: Unexpected token '" << peek().value << "' at position " << current << std::endl;
        return nullptr;
    }
}

// Parse all statements into a BlockStatement.
std::shared_ptr<ASTNode> Parser::parse() {
    std::cout << "Starting parsing..." << std::endl;
    std::vector<std::shared_ptr<ASTNode>> statements;
    while (!isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            statements.push_back(stmt);
        } else {
            std::cerr << "Error: Failed to parse a statement at position " << current << std::endl;
            return nullptr;
        }
    }
    if (statements.empty()) {
        std::cerr << "Error: No valid statements parsed!" << std::endl;
        return nullptr;
    }
    return std::make_shared<BlockStatement>(statements);
}

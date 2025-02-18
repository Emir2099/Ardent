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

/* Parse a simple condition in the form:
      <variable> SURPASSETH <number>
   This function skips over filler tokens such as "the", "fates", "decree", "that". */
std::shared_ptr<ASTNode> Parser::parseSimpleCondition() {
    // Skip filler tokens 
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

// Parse a while loop
std::shared_ptr<ASTNode> Parser::parseWhileLoop() {
    // assume the WHILST token has already been matched.
    Token loopVar = consume(TokenType::IDENTIFIER, "Expected loop variable after 'Whilst the sun doth rise'");
    consume(TokenType::REMAINETH, "Expected 'remaineth below' after loop variable");
    Token limit = consume(TokenType::NUMBER, "Expected numeric limit after 'remaineth below'");
    consume(TokenType::SPOKEN, "Expected 'so shall these words be spoken'");

    // Parse loop body until we detect the start of the increment clause.
    // Here, we skip filler tokens such as "And", "with", "each", "dawn".
    std::vector<std::shared_ptr<ASTNode>> body;
    while (!isAtEnd()) {
        Token next = peek();
        // If the next token is "let", assume it's the start of the increment clause.
        if (next.value == "let") {
            break;
        }
        // If the token is a filler word from the increment clause, skip it.
        if (next.value == "And" || next.value == "with" || next.value == "each" || next.value == "dawn") {
            // Consume all consecutive filler tokens.
            while (!isAtEnd() && (peek().value == "And" || peek().value == "with" ||
                                  peek().value == "each" || peek().value == "dawn")) {
                advance();
            }
            // After skipping, check if we now have "let".
            if (peek().value == "let") break;
            // Otherwise, continue parsing an expression.
        }
        auto expr = parseExpression();
        if (expr) {
            // Wrap the expression in a PrintStatement
            body.push_back(std::make_shared<PrintStatement>(expr));
        } else {
            std::cerr << "Error: Failed to parse expression in while loop body" << std::endl;
        }
    }

    // Now parse the increment clause.
    // It should be in the form: "let <var> ascend <step>"
    Token letToken = consume(TokenType::IDENTIFIER, "Expected 'let' after loop body");
    if (letToken.value != "let") {
        std::cerr << "Error: Expected 'let' but got '" << letToken.value << "'" << std::endl;
        return nullptr;
    }
    Token incVar = consume(TokenType::IDENTIFIER, "Expected loop variable in increment clause");
    if (incVar.value != loopVar.value) {
        std::cerr << "Error: Loop variable in increment clause ('" << incVar.value
                  << "') does not match expected ('" << loopVar.value << "')" << std::endl;
        return nullptr;
    }
    Token ascendToken = consume(TokenType::ASCEND, "Expected 'ascend' after loop variable");
    if (ascendToken.value != "ascend") {
        std::cerr << "Error: Expected 'ascend' but got '" << ascendToken.value << "'" << std::endl;
        return nullptr;
    }
    Token step = consume(TokenType::NUMBER, "Expected increment value after 'ascend'");

    return std::make_shared<WhileLoop>(
        std::make_shared<Expression>(loopVar),
        std::make_shared<Expression>(limit),
        std::make_shared<Expression>(step),
        body
    );
}



std::shared_ptr<ASTNode> Parser::parseForLoop() {
    // After matching "For", parse the loop variable.
    Token loopVarToken = consume(TokenType::IDENTIFIER, "Expected loop variable after 'For'");
    auto loopVar = std::make_shared<Expression>(loopVarToken);

    // Expect "remaineth below"
    consume(TokenType::REMAINETH, "Expected 'remaineth below' after loop variable");

    // Parse the limit
    auto limit = parseExpression();

    // Create condition: <loopVar> remaineth <limit>
    Token op(TokenType::REMAINETH, "remaineth below");
    auto condition = std::make_shared<BinaryExpression>(loopVar, op, limit);

    // Parse the loop body:
    // Expect the token: "so shall these words be spoken"
    consume(TokenType::SPOKEN, "Expected 'so shall these words be spoken'");

    // Now parse the loop body, skipping filler tokens such as "And", "with", "each", "dawn", and commas.
    std::vector<std::shared_ptr<ASTNode>> body;
    while (!isAtEnd() && peek().value != "let") {
        Token next = peek();
        if (next.value == "And" || next.value == "with" || next.value == "each" || 
            next.value == "dawn" || next.value == ",") {
            advance();
            continue;
        }
        auto expr = parseExpression();
        if (expr) {
            // Wrap each expression in a PrintStatement
            body.push_back(std::make_shared<PrintStatement>(expr));
        } else {
            std::cerr << "Error: Failed to parse expression in for loop body" << std::endl;
            break;
        }
    }

    // Parse the increment clause.
    // It should be in the form: "let <var> ascend <step>"
    Token letToken = consume(TokenType::IDENTIFIER, "Expected 'let' at the start of increment clause");
    if (letToken.value != "let") {
        std::cerr << "Error: Expected 'let' but got '" << letToken.value << "'" << std::endl;
        return nullptr;
    }
    Token incVar = consume(TokenType::IDENTIFIER, "Expected loop variable in increment clause");
    if (incVar.value != loopVarToken.value) {
        std::cerr << "Error: Loop variable in increment clause ('" << incVar.value
                  << "') does not match expected ('" << loopVarToken.value << "')" << std::endl;
        return nullptr;
    }
    // Expect the 'ascend' token as type ASCEND.
    Token ascendToken = consume(TokenType::ASCEND, "Expected 'ascend' after loop variable");
    if (ascendToken.value != "ascend") {
        std::cerr << "Error: Expected 'ascend' but got '" << ascendToken.value << "'" << std::endl;
        return nullptr;
    }
    auto step = parseExpression();

    return std::make_shared<ForLoop>(loopVar, condition, step, std::make_shared<BlockStatement>(body));
}

std::shared_ptr<ASTNode> Parser::parseDoWhileLoop() {
    // DO_FATES is already matched.
    // Consume the "so shall these words be spoken" token.
    consume(TokenType::SPOKEN, "Expected 'so shall these words be spoken' after 'Do as the fates decree'");
    
    // Parse the loop body as a single statement.
    auto bodyStmt = parseStatement();
    if (!bodyStmt) {
        std::cerr << "Error: Failed to parse do-while loop body at position " << current << std::endl;
        return nullptr;
    }
    std::shared_ptr<BlockStatement> bodyBlock;
    if (auto block = std::dynamic_pointer_cast<BlockStatement>(bodyStmt)) {
        bodyBlock = block;
    } else {
        bodyBlock = std::make_shared<BlockStatement>(std::vector<std::shared_ptr<ASTNode>>{ bodyStmt });
    }
    
    // Optionally, parse the update clause.
    std::shared_ptr<Expression> updateExpr = nullptr;
    std::shared_ptr<Expression> updateLoopVar = nullptr;
    if (!isAtEnd() && peek().value == "And") {
        // Skip filler tokens until we see "let"
        while (!isAtEnd() && (peek().value == "And" || peek().value == "with" ||
                              peek().value == "each" || peek().value == "dawn" || peek().value == ",")) {
            advance();
        }
        // Now expect "let"
        Token letToken = consume(TokenType::IDENTIFIER, "Expected 'let' at start of update clause in do-while loop");
        if (letToken.value != "let") {
            std::cerr << "Error: Expected 'let' but got '" << letToken.value << "'" << std::endl;
            return nullptr;
        }
        // Consume the loop variable from the update clause.
        Token incVarToken = consume(TokenType::IDENTIFIER, "Expected loop variable in update clause");
        updateLoopVar = std::make_shared<Expression>(incVarToken);
        // Expect the 'ascend' token.
        Token ascendToken = consume(TokenType::ASCEND, "Expected 'ascend' after loop variable in update clause");
        if (ascendToken.value != "ascend") {
            std::cerr << "Error: Expected 'ascend' but got '" << ascendToken.value << "'" << std::endl;
            return nullptr;
        }
        // Parse the increment value.
        updateExpr = std::dynamic_pointer_cast<Expression>(parseExpression());
    }
    
    // Consume the UNTIL token.
    if (!match(TokenType::UNTIL)) {
        std::cerr << "Error: Expected 'Until' after do-while loop body/update at position " << current << std::endl;
        return nullptr;
    }
    
    // Parse the loop condition using parseSimpleCondition() (e.g. "count surpasseth 10")
    auto conditionExpr = parseSimpleCondition();
    if (!conditionExpr) {
        std::cerr << "Error: Failed to parse do-while loop condition at position " << current << std::endl;
        return nullptr;
    }
    // Extract the loop variable from the condition (it should be the left side of the binary expression).
    auto binCond = std::dynamic_pointer_cast<BinaryExpression>(conditionExpr);
    if (!binCond) {
        std::cerr << "Error: Do-while loop condition is not a binary expression" << std::endl;
        return nullptr;
    }
    auto conditionLoopVarExpr = std::dynamic_pointer_cast<Expression>(binCond->left);
    if (!conditionLoopVarExpr) {
        std::cerr << "Error: Could not extract loop variable from do-while condition" << std::endl;
        return nullptr;
    }
    // If an update clause was provided, ensure its loop variable matches the one in the condition.
    if (updateLoopVar && (updateLoopVar->token.value != conditionLoopVarExpr->token.value)) {
        std::cerr << "Error: Loop variable in update clause ('" << updateLoopVar->token.value
                  << "') does not match loop variable in condition ('" << conditionLoopVarExpr->token.value << "')" << std::endl;
        return nullptr;
    }
    
    // Use the loop variable from the condition.
    std::shared_ptr<Expression> loopVarExpr = conditionLoopVarExpr;
    
    return std::make_shared<DoWhileLoop>(bodyBlock, conditionExpr, updateExpr, loopVarExpr);
}



// Parse a single statement.
std::shared_ptr<ASTNode> Parser::parseStatement() {
    if (match(TokenType::LET)) {
        return parseVariableDeclaration();
    } else if (match(TokenType::SHOULD)) {
        return parseIfStatement();
    } else if (match(TokenType::SPELL_NAMED)) {
        return parseFunctionCall();
    } else if (match(TokenType::WHILST)) {
        return parseWhileLoop();
    } else if (match(TokenType::FOR)) {
        return parseForLoop();
    } else if (match(TokenType::DO_FATES)) {
        return parseDoWhileLoop();
    } else if (match(TokenType::LET_PROCLAIMED)) {
        auto expr = parseExpression();
        return std::make_shared<PrintStatement>(expr);
    } else if (peek().type == TokenType::STRING ||
               peek().type == TokenType::NUMBER ||
               peek().type == TokenType::IDENTIFIER) {
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

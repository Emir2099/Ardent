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
    
    // Handle both SURPASSETH and REMAINETH operators
     // Initialize 'op' with dummy values first
    Token op(TokenType::INVALID, "");  // Explicit initialization
    if (peek().type == TokenType::SURPASSETH || peek().type == TokenType::REMAINETH) {
        op = advance(); // Consume the operator token
    } else {
        std::cerr << "Error: Expected 'surpasseth' or 'remaineth below' in condition" << std::endl;
        return nullptr;
    }

    // Parse numeric value (including negative numbers)
    Token right(TokenType::NUMBER, "");
    if (peek().type == TokenType::OPERATOR && peek().value == "-") {
        advance();
        Token num = consume(TokenType::NUMBER, "Expected number after '-'");
        right = Token(TokenType::NUMBER, "-" + num.value);
    } else {
        right = consume(TokenType::NUMBER, "Expected numeric value in condition");
    }

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
    // Skip until we find either a NAMED token or identifier 'named'
    while (!isAtEnd() && !(peek().type == TokenType::NAMED || (peek().type == TokenType::IDENTIFIER && peek().value == "named"))) {
        advance();
    }
    // Consume NAMED (either as token or identifier value)
    if (peek().type == TokenType::NAMED) {
        advance();
    } else if (peek().type == TokenType::IDENTIFIER && peek().value == "named") {
        advance();
    } else {
        std::cerr << "Error: Expected 'named' after 'Let it be known' at position " << current << std::endl;
        return nullptr;
    }
    Token varName = consume(TokenType::IDENTIFIER, "Expected variable name after 'named'");
    consume(TokenType::IS_OF, "Expected 'is of' after variable name");
    // Accept either STRING or NUMBER (with optional leading '-')
    Token value(TokenType::INVALID, "");
    if (peek().type == TokenType::STRING) {
        value = advance();
    } else if (peek().type == TokenType::OPERATOR && peek().value == "-") {
        // negative number literal
        advance();
        Token numTok = consume(TokenType::NUMBER, "Expected number after '-'");
        value = Token(TokenType::NUMBER, "-" + numTok.value);
    } else {
        value = consume(TokenType::NUMBER, "Expected a numeric value or string after 'is of'");
        // Optional unit words like 'winters' or 'years' only apply to numeric declarations
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER) {
            std::string filler = peek().value;
            if (filler == "winters" || filler == "years") {
                advance();
            }
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
    // Assume the WHILST token has already been matched.
    Token loopVarToken = consume(TokenType::IDENTIFIER, "Expected loop variable after 'Whilst the sun doth rise'");
    
    // Check for either SURPASSETH or REMAINETH as the operator
    Token opToken = advance();
    if (opToken.type != TokenType::SURPASSETH && opToken.type != TokenType::REMAINETH) {
        std::cerr << "Error: Expected 'surpasseth' or 'remaineth below' after loop variable, got '" << opToken.value << "'" << std::endl;
        return nullptr;
    }
    
    Token limit = consume(TokenType::NUMBER, "Expected numeric limit after operator");
    consume(TokenType::SPOKEN, "Expected 'so shall these words be spoken'");

    // Parse loop body
    std::vector<std::shared_ptr<ASTNode>> body;
    while (!isAtEnd() && peek().value != "let") {
        // Skip filler tokens
        if (peek().value == "And" || peek().value == "with" || peek().value == "each" || peek().value == "dawn") {
            advance();
            continue;
        }
        auto expr = parseExpression();
        if (expr) {
            body.push_back(std::make_shared<PrintStatement>(expr));
        } else {
            std::cerr << "Error: Failed to parse expression in while loop body" << std::endl;
            break;
        }
    }

    // Parse increment clause
    consume(TokenType::IDENTIFIER, "Expected 'let'"); // Consume 'let'
    Token incVar = consume(TokenType::IDENTIFIER, "Expected loop variable in increment clause");
    if (incVar.value != loopVarToken.value) {
        std::cerr << "Error: Loop variable mismatch in increment clause" << std::endl;
        return nullptr;
    }
    Token stepDirToken = advance(); // ASCEND or DESCEND
    Token stepValue = consume(TokenType::NUMBER, "Expected step value");

    return std::make_shared<WhileLoop>(
        std::make_shared<Expression>(loopVarToken),
        std::make_shared<Expression>(limit),
        std::make_shared<Expression>(stepValue),
        opToken.type, // Pass SURPASSETH/REMAINETH
        stepDirToken.type,
        body
    );
}


// Parse a for loop
std::shared_ptr<ASTNode> Parser::parseForLoop() {
    // After matching "For", parse the loop variable.
    Token loopVarToken = consume(TokenType::IDENTIFIER, "Expected loop variable after 'For'");
    auto loopVar = std::make_shared<Expression>(loopVarToken);
// Expect either "surpasseth" or "remaineth below" 🛠️
Token comparisonOpToken = advance();
if (comparisonOpToken.type != TokenType::SURPASSETH && 
    comparisonOpToken.type != TokenType::REMAINETH) {
    std::cerr << "Error: Expected 'surpasseth' or 'remaineth below' after loop variable\n";
    return nullptr;
}
    // Parse the limit
    auto limit = parseExpression();

    // Create condition: <loopVar> remaineth <limit>
    auto condition = std::make_shared<BinaryExpression>(loopVar, comparisonOpToken, limit);

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
    // It should be in the form: "let <var> ascend <step>" or "let <var> descend <step>"
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
    Token stepToken = advance(); // Consumes either ASCEND or DESCEND
    TokenType stepDir = stepToken.type;
    if (stepDir != TokenType::ASCEND && stepDir != TokenType::DESCEND) {
        std::cerr << "Error: Expected 'ascend' or 'descend' but got '" << stepToken.value << "'" << std::endl;
        return nullptr;
    }
    auto step = parseExpression();

    return std::make_shared<ForLoop>(
        loopVar, condition, step, stepDir, std::make_shared<BlockStatement>(body)
    );
}


// Parse a do-while loop
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
    TokenType stepDir = TokenType::ASCEND; // Default to ASCEND

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
        // Expect the 'ascend' or 'descend' token.
        Token stepToken = advance(); // Consumes either ASCEND or DESCEND
        stepDir = stepToken.type;
        if (stepDir != TokenType::ASCEND && stepDir != TokenType::DESCEND) {
            std::cerr << "Error: Expected 'ascend' or 'descend' but got '" << stepToken.value << "'" << std::endl;
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
    
    return std::make_shared<DoWhileLoop>(bodyBlock, conditionExpr, updateExpr, loopVarExpr, stepDir);
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
        // Special-case: if upcoming identifiers are 'it be proclaimed', treat as a print statement header
        // This allows handling when lexer doesn't emit LET_PROCLAIMED
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "it") {
            size_t save = current;
            // expect: it be proclaimed
            bool ok = true;
            std::vector<std::string> seq = {"it","be","proclaimed"};
            for (const auto& w : seq) {
                if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == w) {
                    advance();
                } else {
                    ok = false; break;
                }
            }
            if (ok) {
                auto expr = parseExpression();
                return std::make_shared<PrintStatement>(expr);
            } else {
                current = save; // restore
            }
        }
        // Otherwise, just treat a bare expression as a print statement
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

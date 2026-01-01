#include "parser.h"
#include "token.h"
#include "ast.h"
#include "types.h"
#include <iostream>
#include <memory>
#include <vector>
#include <sstream>

// Constructor
Parser::Parser(std::vector<Token> tokens) : tokens(tokens), current(0), arena(nullptr) {}

Parser::Parser(std::vector<Token> tokens, Arena* a) : tokens(std::move(tokens)), current(0), arena(a) {}

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

// Logical expression parsing with precedence: NOT > AND > OR
std::shared_ptr<ASTNode> Parser::parseExpression() {
    auto left = parseOr();
    // After logicals, allow arithmetic/operator concatenation as before
    return parseOperatorExpression(0, std::move(left));
}

std::shared_ptr<ASTNode> Parser::parseOr() {
    auto left = parseAnd();
    while (!isAtEnd() && peek().type == TokenType::OR) {
        Token op = advance();
        auto right = parseAnd();
    left = node<BinaryExpression>(left, op, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseAnd() {
    auto left = parseComparison();
    while (!isAtEnd() && peek().type == TokenType::AND) {
        Token op = advance();
        auto right = parseComparison();
    left = node<BinaryExpression>(left, op, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseUnary() {
    if (!isAtEnd() && peek().type == TokenType::NOT) {
        Token op = advance();
        auto operand = parseUnary();
    return node<UnaryExpression>(op, operand);
    }
    // Support unary minus by rewriting to (0 - <operand>)
    if (!isAtEnd() && peek().type == TokenType::OPERATOR && peek().value == "-") {
        Token minus = advance();
        auto operand = parseUnary();
        // Build left literal 0
        Token zeroTok(TokenType::NUMBER, "0");
    auto leftZero = node<Expression>(zeroTok);
    return node<BinaryExpression>(leftZero, Token(TokenType::OPERATOR, "-"), operand);
    }
    if (!isAtEnd() && (peek().type == TokenType::CAST || (peek().type == TokenType::IDENTIFIER && peek().value == "cast"))) {
        return parseCast();
    }
    return parsePrimary();
}

// Parse comparison expressions: handles SURPASSETH, REMAINETH, EQUAL, NOT_EQUAL, GREATER, LESSER
std::shared_ptr<ASTNode> Parser::parseComparison() {
    auto left = parseUnary();
    while (!isAtEnd()) {
        TokenType t = peek().type;
        if (t == TokenType::SURPASSETH || t == TokenType::REMAINETH ||
            t == TokenType::EQUAL || t == TokenType::NOT_EQUAL ||
            t == TokenType::GREATER || t == TokenType::LESSER) {
            Token op = advance();
            auto right = parseUnary();
            left = node<BinaryExpression>(left, op, right);
        } else if (t == TokenType::IDENTIFIER && peek().value == "is") {
            // Attempt to parse literary comparisons from identifier sequence
            size_t save = current;
            advance(); // consume 'is'
            Token op(TokenType::INVALID, "");
            if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "equal") {
                advance();
                if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "to") {
                    advance();
                    op = Token(TokenType::EQUAL, "is equal to");
                }
            } else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "not") {
                advance();
                op = Token(TokenType::NOT_EQUAL, "is not");
            } else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "greater") {
                advance();
                if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "than") {
                    advance();
                    op = Token(TokenType::GREATER, "is greater than");
                }
            } else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "lesser") {
                advance();
                if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "than") {
                    advance();
                    op = Token(TokenType::LESSER, "is lesser than");
                }
            }
            if (op.type != TokenType::INVALID) {
                auto right = parseUnary();
                left = node<BinaryExpression>(left, op, right);
            } else {
                // revert and stop if no comparison phrase recognized
                current = save;
                break;
            }
        } else {
            break;
        }
    }
    return left;
}

// cast <expr> as <type>
std::shared_ptr<ASTNode> Parser::parseCast() {
    // consume 'cast'
    if (peek().type == TokenType::CAST || (peek().type == TokenType::IDENTIFIER && peek().value == "cast")) {
        advance();
    } else {
        return nullptr;
    }
    auto operand = parseUnary();
    // expect 'as'
    if (!(peek().type == TokenType::AS || (peek().type == TokenType::IDENTIFIER && peek().value == "as"))) {
        std::cerr << "Error: Expected 'as' in cast expression" << std::endl;
        return nullptr;
    }
    advance();
    // expect type identifier: phrase | truth | number
    Token typeTok = consume(TokenType::IDENTIFIER, "Expected type name after 'as'");
    CastTarget target = CastTarget::ToPhrase;
    if (typeTok.value == "phrase") target = CastTarget::ToPhrase;
    else if (typeTok.value == "truth") target = CastTarget::ToTruth;
    else if (typeTok.value == "number") target = CastTarget::ToNumber;
    else {
        std::cerr << "Error: Unknown cast target type '" << typeTok.value << "'" << std::endl;
        return nullptr;
    }
    return node<CastExpression>(operand, target);
}

std::shared_ptr<ASTNode> Parser::parsePrimary() {
    Token token = peek();
    std::shared_ptr<ASTNode> cur;
    // Special Chronicle existence pattern: the scroll "path" existeth
    if (token.type == TokenType::IDENTIFIER && token.value == "the") {
        size_t save = current;
        advance();
        bool ok = true;
        if (!( !isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "scroll")) ok = false; else advance();
        if (!( !isAtEnd() && peek().type == TokenType::STRING)) ok = false; 
        Token pathTok = ok ? advance() : Token(TokenType::INVALID, "");
        if (!( !isAtEnd() && peek().type == TokenType::IDENTIFIER && (peek().value == "existeth" || peek().value == "exist"))) ok = false; else advance();
        if (ok) {
            std::vector<std::shared_ptr<ASTNode>> args; args.push_back(node<Expression>(pathTok));
            return node<NativeInvocation>("chronicles.exists", args);
        }
        current = save; // restore if not matched
    }
    if (token.type == TokenType::LBRACKET) {
    cur = parseArrayLiteral();
    } else if (token.type == TokenType::LBRACE) {
    cur = parseObjectLiteral();
    } else if (token.type == TokenType::SPELL_CALL) {
        // Support 'Invoke the spell ...' as an expression
        advance();
    cur = parseSpellInvocation();
    } else if (token.type == TokenType::NATIVE_CALL) {
        // Support 'Invoke the spirit ...' as an expression
        advance();
    cur = parseNativeInvocation();
    } else if (token.type == TokenType::STRING || token.type == TokenType::NUMBER || token.type == TokenType::IDENTIFIER || token.type == TokenType::BOOLEAN) {
        token = advance();  // Consume the token once
        cur = node<Expression>(token);
    } else {
        std::cerr << "Unexpected token: " << token.value << std::endl;
        return nullptr;
    }
    // Postfix indexing: target[expr] ... and dot syntax: target.identifier
    while (!isAtEnd() && (peek().type == TokenType::LBRACKET || peek().type == TokenType::DOT)) {
        if (peek().type == TokenType::LBRACKET) {
            advance(); // consume [
            auto indexExpr = parseExpression();
            if (!match(TokenType::RBRACKET)) {
                std::cerr << "Error: Expected ']' in index expression at position " << current << std::endl;
                return nullptr;
            }
            cur = node<IndexExpression>(cur, indexExpr);
        } else if (peek().type == TokenType::DOT) {
            advance(); // consume '.'
            Token keyTok = consume(TokenType::IDENTIFIER, "Expected identifier after '.' for tome field");
            if (keyTok.type == TokenType::INVALID) return nullptr;
            // Build a STRING literal expression for the key
            auto keyExpr = node<Expression>(Token(TokenType::STRING, keyTok.value));
            cur = node<IndexExpression>(cur, keyExpr);
        }
    }
    return cur;
}

std::shared_ptr<ASTNode> Parser::parseOperatorExpression(int minPrecedence, std::shared_ptr<ASTNode> left) {
    while (true) {
        Token opToken = peek();
        if (opToken.type != TokenType::OPERATOR) break;
        advance();
        // Allow full unary expressions on the right (supports cast, not, literals, identifiers)
    auto right = parseUnary();
    left = node<BinaryExpression>(left, opToken, right);
    }
    return left;
}

// [expr, expr, ...]
std::shared_ptr<ASTNode> Parser::parseArrayLiteral() {
    if (!match(TokenType::LBRACKET)) return nullptr;
    std::vector<std::shared_ptr<ASTNode>> elements;
    if (peek().type != TokenType::RBRACKET) {
        while (true) {
            auto elem = parseExpression();
            if (!elem) return nullptr;
            elements.push_back(elem);
            if (peek().type == TokenType::COMMA) {
                advance();
                continue;
            }
            break;
        }
    }
    if (!match(TokenType::RBRACKET)) {
        std::cerr << "Error: Expected ']' to close array literal at position " << current << std::endl;
        return nullptr;
    }
    return node<ArrayLiteral>(elements);
}

// {"key": expr, ...}
std::shared_ptr<ASTNode> Parser::parseObjectLiteral() {
    if (!match(TokenType::LBRACE)) return nullptr;
    std::vector<std::pair<std::string, std::shared_ptr<ASTNode>>> entries;
    if (peek().type != TokenType::RBRACE) {
        while (true) {
            Token keyTok = consume(TokenType::STRING, "Expected string key in tome literal");
            if (keyTok.type == TokenType::INVALID) return nullptr;
            if (!match(TokenType::COLON)) {
                std::cerr << "Error: Expected ':' after key in tome literal at position " << current << std::endl;
                return nullptr;
            }
            auto val = parseExpression();
            if (!val) return nullptr;
            entries.emplace_back(keyTok.value, val);
            if (peek().type == TokenType::COMMA) {
                advance();
                continue;
            }
            break;
        }
    }
    if (!match(TokenType::RBRACE)) {
        std::cerr << "Error: Expected '}' to close tome literal at position " << current << std::endl;
        return nullptr;
    }
    return node<ObjectLiteral>(entries);
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

    return node<BinaryExpression>(
        node<Expression>(left),
        op,
        node<Expression>(right)
    );
}
// Parse an if-statement.
// Assumes that the SHOULD token has already been matched.
std::shared_ptr<ASTNode> Parser::parseIfStatement() {
    // Skip filler words first
    while (!isAtEnd() && 
           (peek().type == TokenType::FATES ||
            peek().type == TokenType::DECREE ||
            (peek().type == TokenType::IDENTIFIER && (peek().value == "the" || peek().value == "that")))) {
        advance();
    }
    size_t save = current;
    auto condition = parseExpression();
    if (!match(TokenType::THEN)) {
        // fallback to numeric simple condition
        current = save;
        condition = parseSimpleCondition();
        if (!match(TokenType::THEN)) {
            std::cerr << "Error: Expected THEN after IF condition at position " << current << std::endl;
            return nullptr;
        }
    }
    // For the then-branch: if the next token is LET_PROCLAIMED, treat it as a print statement.
    std::shared_ptr<ASTNode> thenBranch;
    if (match(TokenType::LET_PROCLAIMED)) {
        auto expr = parseExpression();
    thenBranch = node<PrintStatement>(expr);
    } else {
        thenBranch = parseExpression();
    }
    std::shared_ptr<ASTNode> elseBranch = nullptr;
    if (match(TokenType::ELSE)) {
        if (match(TokenType::WHISPER)) {
            auto expr = parseExpression();
            elseBranch = node<PrintStatement>(expr);
        } else {
            elseBranch = parseExpression();
        }
    }
    return node<IfStatement>(condition, thenBranch, elseBranch);
}

// Stub for function calls.
std::shared_ptr<ASTNode> Parser::parseFunctionCall() {
    // Implementation for function calls goes here.
    return nullptr;
}

// Parse a variable declaration.
// After encountering LET, skip filler tokens until we find NAMED.
// 2.2 Type Runes: supports name:type syntax (e.g., x:whole, name:phrase)
// Also supports short-form: let x:type be value
std::shared_ptr<ASTNode> Parser::parseVariableDeclaration() {
    ardent::Type declaredType = ardent::Type::unknown();
    
    // 2.2 Short-form syntax: let <name>[:type] be <value>
    // Check if next token is directly an identifier (short form)
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER) {
        Token varNameTok = advance();
        std::string varName = varNameTok.value;
        
        // Check for :type rune after variable name
        if (!isAtEnd() && peek().type == TokenType::COLON) {
            advance(); // consume ':'
            Token typeTok = consume(TokenType::IDENTIFIER, "Expected type name after ':'");
            if (typeTok.type != TokenType::INVALID) {
                auto parsedType = ardent::parseTypeRune(typeTok.value);
                if (parsedType) {
                    declaredType = *parsedType;
                } else {
                    std::cerr << "Warning: Unknown type rune '" << typeTok.value 
                              << "' at position " << current << ", treating as dynamic" << std::endl;
                }
            }
        }
        
        // Check for 'be' keyword
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "be") {
            advance(); // consume 'be'
            
            // Parse RHS expression
            auto rhsNode = parseExpression();
            
            // Return VariableDeclaration node
            return node<VariableDeclaration>(varName, rhsNode, declaredType);
        }
        
        // Not short form - rewind and try verbose form
        current = current - 1; // back to identifier
        if (declaredType.kind != ardent::TypeKind::Unknown) {
            current -= 2; // also back past : and type
        }
    }
    
    // Verbose/legacy form: Skip until we find either a NAMED token or identifier 'named'
    while (!isAtEnd() && !(peek().type == TokenType::NAMED || (peek().type == TokenType::IDENTIFIER && peek().value == "named"))) {
        advance();
    }
    // Track declared type from the NAMED token when available (legacy syntax)
    // declaredType already declared above
    if (peek().type == TokenType::NAMED) {
        Token namedTok = advance();
        if (namedTok.value == "a number named") declaredType = ardent::Type::whole();
        else if (namedTok.value == "a phrase named") declaredType = ardent::Type::phrase();
        else if (namedTok.value == "a truth named") declaredType = ardent::Type::truth();
        else if (namedTok.value == "an order named") declaredType = ardent::Type::order(ardent::Type::unknown());
        else if (namedTok.value == "a tome named") declaredType = ardent::Type::tome(ardent::Type::unknown(), ardent::Type::unknown());
    } else if (peek().type == TokenType::IDENTIFIER && peek().value == "named") {
        advance();
    } else {
        std::cerr << "Error: Expected 'named' after 'Let it be known' at position " << current << std::endl;
        return nullptr;
    }
    Token varName = consume(TokenType::IDENTIFIER, "Expected variable name after 'named'");
    
    // 2.2 Type Runes: check for :type syntax after variable name
    if (!isAtEnd() && peek().type == TokenType::COLON) {
        advance(); // consume ':'
        Token typeTok = consume(TokenType::IDENTIFIER, "Expected type name after ':'");
        if (typeTok.type != TokenType::INVALID) {
            auto parsedType = ardent::parseTypeRune(typeTok.value);
            if (parsedType) {
                // Type rune overrides legacy "a number named" syntax
                declaredType = *parsedType;
            } else {
                std::cerr << "Warning: Unknown type rune '" << typeTok.value 
                          << "' at position " << current << ", treating as dynamic" << std::endl;
            }
        }
    }
    
    consume(TokenType::IS_OF, "Expected 'is of' after variable name");
    // Accept either a literal (STRING/BOOLEAN/NUMBER), a Chronicle read, or a full expression
    std::shared_ptr<ASTNode> rhsNode;
    bool simpleLiteral = false;
    Token value(TokenType::INVALID, "");
    if (match(TokenType::READING_FROM) || (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "reading" && (advance(), !isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "from" && (advance(), true)))) {
    Token pathTok = consume(TokenType::STRING, "Expected path after 'reading from'");
    std::vector<std::shared_ptr<ASTNode>> args; args.push_back(node<Expression>(pathTok));
    rhsNode = node<NativeInvocation>("chronicles.read", args);
    }
    else if (peek().type == TokenType::STRING) {
        value = advance(); simpleLiteral = true;
    rhsNode = node<Expression>(value);
    } else if (peek().type == TokenType::BOOLEAN) {
        value = advance(); simpleLiteral = true;
    rhsNode = node<Expression>(value);
    } else if (peek().type == TokenType::OPERATOR && peek().value == "-") {
        advance();
        Token numTok = consume(TokenType::NUMBER, "Expected number after '-'");
    value = Token(TokenType::NUMBER, "-" + numTok.value); simpleLiteral = true;
    rhsNode = node<Expression>(value);
    } else if (peek().type == TokenType::NUMBER) {
    value = advance(); simpleLiteral = true;
    rhsNode = node<Expression>(value);
    } else {
        rhsNode = parseExpression();
        if (!rhsNode) {
            std::cerr << "Error: Failed to parse expression after 'is of' at position " << current << std::endl;
            return nullptr;
        }
    }
    // Optional unit words like 'winters' or 'years' (ignore if present)
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER) {
        std::string filler = peek().value;
        if (filler == "winters" || filler == "years") {
            advance();
        }
    }

    // Enforce declared type only for simple literal cases (expressions validated at runtime)
    if (simpleLiteral && declaredType.isKnown()) {
        auto mismatch = [&](const char* expected) {
            std::cerr << "TypeError: The rune declares " << varName.value << " as " << expected
                      << ", yet fate reveals a " << tokenTypeToString(value.type) << " at this position." << std::endl;
        };
        if (declaredType.isNumeric() && value.type != TokenType::NUMBER) {
            mismatch("whole");
            return nullptr;
        }
        if (declaredType.isString() && value.type != TokenType::STRING) {
            mismatch("phrase");
            return nullptr;
        }
        if (declaredType.isBoolean() && value.type != TokenType::BOOLEAN) {
            mismatch("truth");
            return nullptr;
        }
    }
    
    // Return proper VariableDeclaration node (2.2)
    return node<VariableDeclaration>(varName.value, rhsNode, declaredType);
}

// Parse a while loop
// TODO(Phase VI): support multi-statement while bodies and post-loop proclamations
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
            body.push_back(node<PrintStatement>(expr));
        } else {
            // Standardize error so tests can assert on unsupported while body patterns
            std::cerr << "Error: Unexpected token or missing block while parsing while loop body" << std::endl;
            return nullptr; // abort parsing this while loop
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

    return node<WhileLoop>(
        node<Expression>(loopVarToken),
        node<Expression>(limit),
        node<Expression>(stepValue),
        opToken.type, // Pass SURPASSETH/REMAINETH
        stepDirToken.type,
        body
    );
}


// Parse a for loop
std::shared_ptr<ASTNode> Parser::parseForLoop() {
    // After matching "For", parse the loop variable.
    Token loopVarToken = consume(TokenType::IDENTIFIER, "Expected loop variable after 'For'");
    auto loopVar = node<Expression>(loopVarToken);
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
    auto condition = node<BinaryExpression>(loopVar, comparisonOpToken, limit);

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
            body.push_back(node<PrintStatement>(expr));
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

    return node<ForLoop>(
        loopVar, condition, step, stepDir, node<BlockStatement>(body)
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
    bodyBlock = node<BlockStatement>(std::vector<std::shared_ptr<ASTNode>>{ bodyStmt });
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
    updateLoopVar = node<Expression>(incVarToken);
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
    
    return node<DoWhileLoop>(bodyBlock, conditionExpr, updateExpr, loopVarExpr, stepDir);
}



// Parse a single statement.
std::shared_ptr<ASTNode> Parser::parseStatement() {
    if (match(TokenType::SPELL_DEF)) {
        return parseSpellDefinition();
    } else if (match(TokenType::SPELL_CALL)) {
        return parseSpellInvocation();
    } else if (match(TokenType::NATIVE_CALL)) {
        return parseNativeInvocation();
    } else if (match(TokenType::TRY)) {
        return parseTryCatch();
    } else if (match(TokenType::FROM_SCROLL)) {
        return parseImportStatement();
    } else if (match(TokenType::UNFURL_SCROLL)) {
        return parseUnfurl();
    } else if (match(TokenType::LET)) {
        // Peek ahead for collection rites: 'the order' / 'the tome' <name> verb ...
        size_t save = current;
        // Accept optional 'the'
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && (peek().value == "the")) {
            advance();
        }
        bool isCollection = false;
        bool isOrder = false;
        bool isTome = false;
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && (peek().value == "order")) { isCollection = true; isOrder = true; advance(); }
        else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && (peek().value == "tome")) { isCollection = true; isTome = true; advance(); }
        if (isCollection) {
            // variable name
            Token varTok = consume(TokenType::IDENTIFIER, "Expected collection variable name after 'order'/'tome'");
            if (varTok.type == TokenType::INVALID) return nullptr;
            // verb
            Token verb = peek();
            if (verb.type == TokenType::EXPAND || verb.type == TokenType::AMEND || verb.type == TokenType::REMOVE || verb.type == TokenType::ERASE) {
                advance();
                if (verb.type == TokenType::EXPAND && isOrder) {
                    // expand with <expr>
                    if (!match(TokenType::IDENTIFIER) || tokens[current-1].value != "with") {
                        std::cerr << "Error: Expected 'with' after expand" << std::endl; return nullptr; }
                    auto elemExpr = parseExpression();
                    return node<CollectionRite>(CollectionRiteType::OrderExpand, varTok.value, nullptr, elemExpr);
                } else if (verb.type == TokenType::AMEND && isTome) {
                    // amend "key" to <expr>
                    auto keyNode = parseExpression();
                    if (!match(TokenType::IDENTIFIER) || tokens[current-1].value != "to") { std::cerr << "Error: Expected 'to' after amend key" << std::endl; return nullptr; }
                    auto valExpr = parseExpression();
                    return node<CollectionRite>(CollectionRiteType::TomeAmend, varTok.value, keyNode, valExpr);
                } else if (verb.type == TokenType::REMOVE && isOrder) {
                    // remove <expr> (element match by equality)
                    auto elemExpr = parseExpression();
                    return node<CollectionRite>(CollectionRiteType::OrderRemove, varTok.value, elemExpr, nullptr);
                } else if (verb.type == TokenType::ERASE && isTome) {
                    // erase "key"
                    auto keyNode = parseExpression();
                    return node<CollectionRite>(CollectionRiteType::TomeErase, varTok.value, keyNode, nullptr);
                } else {
                    std::cerr << "Error: Rite verb incompatible with collection type" << std::endl; return nullptr;
                }
            }
            // Not a rite; revert and parse as declaration
            current = save;
        }
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
    return node<PrintStatement>(expr);
    } else if (match(TokenType::INSCRIBE)) {
        return parseInscribe(false);
    } else if (match(TokenType::ETCH)) {
        return parseInscribe(true);
    } else if (match(TokenType::BANISH)) {
        return parseBanish();
    } else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "From") {
        // Fallback wordy import: From the scroll of "path" (draw all knowledge [as alias] | take [the] [spells] ...)
        // Consume 'From the scroll of'
        advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "scroll") advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "of") advance();
        Token pathTok = consume(TokenType::STRING, "Expected scroll path after 'From the scroll of'");
        // Decide branch: draw all knowledge [as alias] OR take ...
        bool isDrawAll = false;
        if (!isAtEnd() && peek().type == TokenType::DRAW_ALL_KNOWLEDGE) { advance(); isDrawAll = true; }
        else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "draw") {
            // accept identifiers 'draw all knowledge'
            advance();
            if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "all") advance();
            if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "knowledge") { advance(); isDrawAll = true; }
        }
        if (isDrawAll) {
            std::string alias;
            if (!isAtEnd() && peek().type == TokenType::AS) {
                advance();
                Token aliasTok = consume(TokenType::IDENTIFIER, "Expected alias name after 'as'");
                alias = aliasTok.value;
            }
            if (!isAtEnd() && peek().type == TokenType::DOT) advance();
            return node<ImportAll>(pathTok.value, alias);
        }
        // Else 'take' branch
        bool sawTake = false;
        if (!isAtEnd() && peek().type == TokenType::TAKE) { advance(); sawTake = true; }
        else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "take") { advance(); sawTake = true; }
        if (!sawTake) { std::cerr << "Error: Expected 'draw all knowledge' or 'take' after scroll path" << std::endl; return nullptr; }
        // Optionally consume 'the' 'spells'
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && (peek().value == "spells" || peek().value == "spell")) advance();
        std::vector<std::string> names;
        while (!isAtEnd()) {
            if (peek().type != TokenType::IDENTIFIER) break;
            Token t = advance();
            names.push_back(t.value);
            if (peek().type == TokenType::COMMA) { advance(); continue; }
            break;
        }
        if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    return node<ImportSelective>(pathTok.value, names);
    } else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "Unfurl") {
        // Fallback wordy unfurl: Unfurl the scroll "path".
        advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "scroll") advance();
        Token pathTok = consume(TokenType::STRING, "Expected scroll path after 'Unfurl the scroll'");
        if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    return node<UnfurlInclude>(pathTok.value);
    } else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "Try") {
        // Fallback wordy Try
        advance();
        return parseTryCatch();
    } else if (match(TokenType::AWAIT)) {
        // "Await the omen of <expr>"
        return parseAwaitExpression();
    } else if (match(TokenType::SCRIBE)) {
        // "Let a scribe be opened upon <path>"
        return parseScribeDeclaration();
    } else if (match(TokenType::WRITE_INTO)) {
        // "Write the verse <expr> into <scribe>"
        return parseStreamWrite();
    } else if (match(TokenType::CLOSE)) {
        // "Close the scribe <name>"
        return parseStreamClose();
    } else if (match(TokenType::READ_FROM_STREAM)) {
        // "Read from scribe <name> line by line as <var>"
        return parseStreamReadLoop();
    } else if (
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
                return node<PrintStatement>(expr);
            } else {
                current = save; // restore
            }
        }
        // Otherwise, parse a bare expression, but guard against illegal assignment into collections
        auto expr = parseExpression();
        if (!isAtEnd() && peek().type == TokenType::IS_OF) {
            // Attempt to assign outside declaration; if LHS is an index expression, enforce immutability rule
            if (std::dynamic_pointer_cast<IndexExpression>(expr)) {
                std::cerr << "Error: Immutable rite: one may not assign into an order or tome; speak 'expand' or 'amend' instead." << std::endl;
            } else {
                std::cerr << "Error: Assignment outside declaration is not supported." << std::endl;
            }
            return nullptr;
        }
    return node<PrintStatement>(expr);
    } else {
        std::cerr << "Error: Unexpected token '" << peek().value << "' at position " << current << std::endl;
        return nullptr;
    }
}


// Parse all statements into a BlockStatement.
std::shared_ptr<ASTNode> Parser::parse() {
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
    return node<BlockStatement>(statements);
}

// Parse a spell definition:
// By decree of the elders, a spell named greet is cast upon a traveler known as name:
//     <body statements>
std::shared_ptr<ASTNode> Parser::parseSpellDefinition() {
    // Expect optional comma after phrase already consumed as SPELL_DEF token.
    // Consume optional 'a' or determiners until we reach SPELL_NAMED
    while (!isAtEnd() && peek().type != TokenType::SPELL_NAMED) advance();
    if (!match(TokenType::SPELL_NAMED)) { std::cerr << "Error: Expected 'spell named' in spell definition" << std::endl; return nullptr; }
    // Accept identifier or certain keywords as valid spell names (e.g., 'whisper')
    while (!isAtEnd() && !(peek().type == TokenType::IDENTIFIER || peek().type == TokenType::WHISPER)) advance();
    Token spellName = peek();
    if (spellName.type == TokenType::IDENTIFIER || spellName.type == TokenType::WHISPER) {
        advance();
    } else {
        consume(TokenType::IDENTIFIER, "Expected spell name after 'spell named'");
        return nullptr;
    }
    
    // 2.2 Type Runes: check for return type annotation after spell name (name:returnType)
    ardent::Type returnType = ardent::Type::unknown();
    if (!isAtEnd() && peek().type == TokenType::COLON) {
        // This could be either :returnType or the body start colon
        // Peek ahead: if next is identifier that's a type name, it's the return type
        size_t save = current;
        advance(); // consume ':'
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER) {
            auto maybeType = ardent::parseTypeRune(peek().value);
            if (maybeType) {
                advance(); // consume type name
                returnType = *maybeType;
            } else {
                // Not a type, restore position (it's the body colon)
                current = save;
            }
        } else {
            current = save;
        }
    }
    
    // Expect 'is cast upon'
    if (!match(TokenType::SPELL_CAST)) { std::cerr << "Error: Expected 'is cast upon' in spell definition" << std::endl; return nullptr; }
    // Parameter parsing: sequence of descriptor tokens ending with KNOWN_AS name pairs until ':'
    std::vector<std::string> params;
    std::vector<ardent::Type> paramTypes; // 2.2: param types
    while (!isAtEnd() && peek().type != TokenType::COLON) {
        // Consume descriptor words until KNOWN_AS
        while (!isAtEnd() && peek().type != TokenType::KNOWN_AS && peek().type != TokenType::COLON) {
            advance();
        }
        if (peek().type == TokenType::COLON) break;
        if (!match(TokenType::KNOWN_AS)) { std::cerr << "Error: Expected 'known as' before parameter name" << std::endl; return nullptr; }
        Token paramName = consume(TokenType::IDENTIFIER, "Expected parameter name after 'known as'");
        
        // 2.2 Type Runes: check for :type after param name
        // Must distinguish between "name:type" and "name:" (body separator)
        ardent::Type paramType = ardent::Type::unknown();
        if (!isAtEnd() && peek().type == TokenType::COLON) {
            // Peek ahead: if the next token after COLON is an identifier that's a valid type, consume it
            size_t save = current;
            advance(); // consume ':'
            if (!isAtEnd() && peek().type == TokenType::IDENTIFIER) {
                auto maybeType = ardent::parseTypeRune(peek().value);
                if (maybeType) {
                    paramType = *maybeType;
                    advance(); // consume type name
                } else {
                    // Not a type, it's the body separator colon - restore position
                    current = save;
                }
            } else {
                // Nothing after colon means it's the body separator
                current = save;
            }
        }
        
        params.push_back(paramName.value);
        paramTypes.push_back(paramType);
    }
    consume(TokenType::COLON, "Expected ':' to start spell body");
    // Parse indented body lines until next SPELL_DEF/SPELL_CALL or end. We'll accumulate statements until a blank or termination.
    std::vector<std::shared_ptr<ASTNode>> bodyStmts;
    bool seenReturn = false;
    while (!isAtEnd()) {
        // Stop if next token begins another top-level construct
        TokenType t = peek().type;
        if (t == TokenType::SPELL_DEF || t == TokenType::SPELL_CALL || t == TokenType::SHOULD) {
            break;
        }
        // 2.2: Stop if we see short-form 'let' AFTER a return statement 
        // This indicates top-level code after the spell definition
        if (seenReturn && t == TokenType::LET && peek().value == "let") {
            break;
        }
        if (t == TokenType::LET_PROCLAIMED) {
            // Heuristic: stop body if this proclamation is immediately a top-level spell invocation
            if (current + 1 < tokens.size() && tokens[current + 1].type == TokenType::SPELL_CALL) {
                break;
            }
            // Also stop if we've seen a return (top-level print after spell)
            if (seenReturn) {
                break;
            }
        }
        if (t == TokenType::RETURN) {
            advance(); // consume RETURN
            auto retExpr = parseExpression();
            bodyStmts.push_back(node<ReturnStatement>(retExpr));
            seenReturn = true;
            // After a return, optionally continue to allow trailing statements, but they won't execute after return.
            continue;
        }
        bodyStmts.push_back(parseStatement());
    }
    auto block = node<BlockStatement>(bodyStmts);
    return node<SpellStatement>(spellName.value, params, paramTypes, returnType, block);
}

// Parse invocation: Invoke the spell greet upon "Aragorn".
std::shared_ptr<ASTNode> Parser::parseSpellInvocation() {
    // Expect spell name after SPELL_CALL
    // skip optional whitespace/determiners until IDENTIFIER
    while (!isAtEnd() && !(peek().type == TokenType::IDENTIFIER || peek().type == TokenType::WHISPER)) advance();
    // Accept reserved keyword 'whisper' as a valid spell name to avoid collision with Else-branch whispering
    Token nameTok = peek();
    if (nameTok.type == TokenType::IDENTIFIER || nameTok.type == TokenType::WHISPER) {
        advance();
    } else {
        consume(TokenType::IDENTIFIER, "Expected spell name after 'Invoke the spell'");
        return nullptr;
    }
    // Support dotted spell names e.g., alch.transmute
    std::string fullName = nameTok.value;
    while (!isAtEnd() && peek().type == TokenType::DOT) {
        advance();
        Token part = consume(TokenType::IDENTIFIER, "Expected identifier after '.' in qualified spell name");
        fullName += "." + part.value;
    }
    consume(TokenType::UPON, "Expected 'upon' after spell name");
    // Single or comma-separated arguments until period or LET or end
    std::vector<std::shared_ptr<ASTNode>> args;
    while (!isAtEnd()) {
        // Stop on LET / SPELL_DEF / SPELL_CALL to avoid consuming next statement
        TokenType t = peek().type;
        if (t == TokenType::LET || t == TokenType::SPELL_DEF || t == TokenType::SPELL_CALL) break;
        // Break if a structural token that likely ends argument list
        if (t == TokenType::WHISPER || t == TokenType::SHOULD) break;
        // Parse one arg expression
        auto expr = parseExpression();
        if (expr) args.push_back(expr); else break;
        // Optional comma token
        if (peek().type == TokenType::COMMA) advance(); else break;
    }
    return node<SpellInvocation>(fullName, args);
}

// Parse native invocation: Invoke the spirit [of] name[.qual] upon args
std::shared_ptr<ASTNode> Parser::parseNativeInvocation() {
    // Skip optional determiners until first IDENTIFIER (handle optional 'of')
    while (!isAtEnd() && !(peek().type == TokenType::IDENTIFIER)) advance();
    // Accept and skip an initial 'of' if present
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "of") {
        advance();
    }
    // Now expect the function name identifier
    Token nameTok = consume(TokenType::IDENTIFIER, "Expected function name after 'Invoke the spirit'");
    std::string fullName = nameTok.value;
    while (!isAtEnd() && peek().type == TokenType::DOT) {
        advance();
        Token part = consume(TokenType::IDENTIFIER, "Expected identifier after '.' in qualified function name");
        fullName += "." + part.value;
    }
    consume(TokenType::UPON, "Expected 'upon' after function name");
    std::vector<std::shared_ptr<ASTNode>> args;
    while (!isAtEnd()) {
        TokenType t = peek().type;
        if (t == TokenType::LET || t == TokenType::SPELL_DEF || t == TokenType::SPELL_CALL || t == TokenType::NATIVE_CALL) break;
        if (t == TokenType::WHISPER || t == TokenType::SHOULD) break;
        auto expr = parseExpression();
        if (expr) args.push_back(expr); else break;
        if (peek().type == TokenType::COMMA) advance(); else break;
    }
    return node<NativeInvocation>(fullName, args);
}

// From the scroll of "path" draw all knowledge [as alias].
// From the scroll of "path" take [the] [spells] name[, name]* .
std::shared_ptr<ASTNode> Parser::parseImportStatement() {
    // Expect STRING path
    Token pathTok = consume(TokenType::STRING, "Expected scroll path after 'From the scroll of'");
    bool drawAll = false;
    if (match(TokenType::DRAW_ALL_KNOWLEDGE)) { drawAll = true; }
    else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "draw") {
        // Accept identifiers 'draw all knowledge'
        advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "all") advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "knowledge") { advance(); drawAll = true; }
    }
    if (drawAll) {
        std::string alias;
        if (!isAtEnd() && peek().type == TokenType::AS) {
            advance();
            Token aliasTok = consume(TokenType::IDENTIFIER, "Expected alias name after 'as'");
            alias = aliasTok.value;
        }
        // Optional trailing period
        if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    return node<ImportAll>(pathTok.value, alias);
    }
    bool sawTake = false;
    if (match(TokenType::TAKE)) { sawTake = true; }
    else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "take") { advance(); sawTake = true; }
    if (sawTake) {
        // Optionally consume 'the' 'spells'
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && (peek().value == "spells" || peek().value == "spell")) advance();
        std::vector<std::string> names;
        while (!isAtEnd()) {
            Token t = consume(TokenType::IDENTIFIER, "Expected spell name in take list");
            names.push_back(t.value);
            if (peek().type == TokenType::COMMA) { advance(); continue; }
            break;
        }
        if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    return node<ImportSelective>(pathTok.value, names);
    }
    std::cerr << "Error: Expected 'draw all knowledge' or 'take' after scroll path" << std::endl;
    return nullptr;
}

// Try: block; optional Catch the curse as <id>: block; optional Finally: block
std::shared_ptr<ASTNode> Parser::parseTryCatch() {
    // Optional ':' after Try
    if (peek().type == TokenType::COLON) advance();
    auto parseBlockUntil = [&](const std::vector<TokenType>& stops) -> std::shared_ptr<BlockStatement> {
        std::vector<std::shared_ptr<ASTNode>> statements;
        while (!isAtEnd()) {
            TokenType t = peek().type;
            bool stop = false;
            for (auto s : stops) { if (t == s) { stop = true; break; } }
            if (t == TokenType::SPELL_DEF || t == TokenType::SPELL_CALL || t == TokenType::NATIVE_CALL || t == TokenType::FROM_SCROLL || t == TokenType::UNFURL_SCROLL) {
                // Do not swallow other top-level constructs unless part of block explicitly
            }
            if (stop) break;
            // Stop as well if identifier tokens announce Catch/Finally in wordy form
            if (t == TokenType::IDENTIFIER && (peek().value == "Catch" || peek().value == "Finally")) break;
            auto stmt = parseStatement();
            if (!stmt) { std::cerr << "Error: Failed to parse statement within try/catch block" << std::endl; return nullptr; }
            statements.push_back(stmt);
        }
    return node<BlockStatement>(statements);
    };

    auto tryBlock = parseBlockUntil({TokenType::CATCH, TokenType::FINALLY});
    if (!tryBlock) return nullptr;

    std::string catchVar;
    std::shared_ptr<BlockStatement> catchBlock = nullptr;
    if (match(TokenType::CATCH) || (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "Catch" && (advance(), true))) {
        // Accept optional 'the curse as <id>'
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && (peek().value == "curse" || peek().value == "curses")) advance();
        if (!isAtEnd() && ((peek().type == TokenType::IDENTIFIER && peek().value == "as") || peek().type == TokenType::AS)) {
            advance();
            Token varTok = consume(TokenType::IDENTIFIER, "Expected catch variable after 'as'");
            if (varTok.type == TokenType::INVALID) return nullptr;
            catchVar = varTok.value;
        }
    if (peek().type == TokenType::COLON) advance();
    // Stop the catch block when encountering either a new 'Catch' (outer) or 'Finally'
    catchBlock = parseBlockUntil({TokenType::FINALLY, TokenType::CATCH});
        if (!catchBlock) return nullptr;
    }

    std::shared_ptr<BlockStatement> finallyBlock = nullptr;
    if (match(TokenType::FINALLY) || (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "Finally" && (advance(), true))) {
        if (peek().type == TokenType::COLON) advance();
        finallyBlock = parseBlockUntil({});
        if (!finallyBlock) return nullptr;
    }
    return node<TryCatch>(tryBlock, catchVar, catchBlock, finallyBlock);
}

// Unfurl the scroll "path".
std::shared_ptr<ASTNode> Parser::parseUnfurl() {
    Token pathTok = consume(TokenType::STRING, "Expected scroll path after 'Unfurl the scroll'");
    if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    return node<UnfurlInclude>(pathTok.value);
}

// Chronicle Rites: Inscribe/Etch upon "path" the words <expr>
std::shared_ptr<ASTNode> Parser::parseInscribe(bool append) {
    // Accept optional 'upon'
    if (!isAtEnd() && peek().type == TokenType::UPON) advance();
    Token pathTok = consume(TokenType::STRING, "Expected path after 'upon'");
    // Optional 'the words'
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && (peek().value == "words" || peek().value == "word")) advance();
    auto contentExpr = parseExpression();
    if (!contentExpr) return nullptr;
    std::vector<std::shared_ptr<ASTNode>> args;
    args.push_back(node<Expression>(pathTok));
    args.push_back(contentExpr);
    return node<NativeInvocation>(append ? "chronicles.append" : "chronicles.write", args);
}

// Banish the scroll "path".
std::shared_ptr<ASTNode> Parser::parseBanish() {
    // In lexer we consumed 'Banish the scroll', so just expect path
    // Skip optional words 'the scroll'
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "scroll") advance();
    Token pathTok = consume(TokenType::STRING, "Expected path after 'Banish' rite");
    if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    // Defensive: if an extra stray string token remains (e.g., lexing quirk), consume it
    if (!isAtEnd() && peek().type == TokenType::STRING) advance();
    std::vector<std::shared_ptr<ASTNode>> args; args.push_back(node<Expression>(pathTok));
    return node<NativeInvocation>("chronicles.delete", args);
}

// ============================================================================
// ASYNC / STREAM PARSING (2.4 Living Chronicles)
// ============================================================================

// Parse "Await the omen of <expr>"
// The AWAIT token was already consumed
std::shared_ptr<ASTNode> Parser::parseAwaitExpression() {
    // Skip optional "the omen of" words if they appear as identifiers
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "omen") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "of") advance();
    
    auto expr = parseExpression();
    if (!expr) {
        std::cerr << "Error: Expected expression after 'Await'" << std::endl;
        return nullptr;
    }
    return node<AwaitExpression>(expr);
}

// Parse "Let a scribe <name> be opened upon <path> [for reading/writing/appending]"
// The SCRIBE token was already consumed
std::shared_ptr<ASTNode> Parser::parseScribeDeclaration() {
    // Expect scribe name
    Token nameTok = consume(TokenType::IDENTIFIER, "Expected scribe name after 'Let a scribe'");
    if (nameTok.type == TokenType::INVALID) return nullptr;
    
    // Skip "be opened upon" words
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "be") advance();
    if (!isAtEnd() && peek().type == TokenType::OPENED) advance();
    else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "opened") advance();
    if (!isAtEnd() && peek().type == TokenType::UPON) advance();
    else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "upon") advance();
    
    // Expect path expression
    auto pathExpr = parseExpression();
    if (!pathExpr) {
        std::cerr << "Error: Expected path expression after 'upon'" << std::endl;
        return nullptr;
    }
    
    // Check for mode: "for reading", "for writing", "for appending"
    std::string mode = "read";  // default mode
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "for") {
        advance();
        if (!isAtEnd() && peek().type == TokenType::IDENTIFIER) {
            std::string modeWord = peek().value;
            advance();
            if (modeWord == "reading") mode = "read";
            else if (modeWord == "writing") mode = "write";
            else if (modeWord == "appending") mode = "append";
            else if (modeWord == "both" || modeWord == "all") mode = "readwrite";
        }
    }
    
    // Consume trailing period if present
    if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    
    return node<ScribeDeclaration>(nameTok.value, pathExpr, mode);
}

// Parse "Write the verse <expr> into <scribe>"
// The WRITE_INTO token was already consumed
std::shared_ptr<ASTNode> Parser::parseStreamWrite() {
    // Skip optional "the verse" / "the words"
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && 
        (peek().value == "verse" || peek().value == "words" || peek().value == "text")) advance();
    
    // Parse the content expression
    auto contentExpr = parseExpression();
    if (!contentExpr) {
        std::cerr << "Error: Expected expression after 'Write'" << std::endl;
        return nullptr;
    }
    
    // Expect "into <scribe>"
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "into") advance();
    else if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "unto") advance();
    
    Token scribeTok = consume(TokenType::IDENTIFIER, "Expected scribe name after 'into'");
    if (scribeTok.type == TokenType::INVALID) return nullptr;
    
    // Consume trailing period if present
    if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    
    return node<StreamWriteStatement>(scribeTok.value, contentExpr);
}

// Parse "Close the scribe <name>"
// The CLOSE token was already consumed
std::shared_ptr<ASTNode> Parser::parseStreamClose() {
    // Skip optional "the scribe"
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "the") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "scribe") advance();
    
    Token scribeTok = consume(TokenType::IDENTIFIER, "Expected scribe name after 'Close'");
    if (scribeTok.type == TokenType::INVALID) return nullptr;
    
    // Consume trailing period if present
    if (!isAtEnd() && peek().type == TokenType::DOT) advance();
    
    return node<StreamCloseStatement>(scribeTok.value);
}

// Parse "Read from scribe <name> line by line as <var>: <body>"
// The READ_FROM_STREAM token was already consumed
std::shared_ptr<ASTNode> Parser::parseStreamReadLoop() {
    // Skip optional "from scribe"
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "from") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "scribe") advance();
    
    Token scribeTok = consume(TokenType::IDENTIFIER, "Expected scribe name after 'Read from scribe'");
    if (scribeTok.type == TokenType::INVALID) return nullptr;
    
    // Skip "line by line"
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "line") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "by") advance();
    if (!isAtEnd() && peek().type == TokenType::IDENTIFIER && peek().value == "line") advance();
    
    // Expect "as <var>"
    std::string lineVar = "line";  // default variable name
    if (!isAtEnd() && (peek().type == TokenType::AS || 
        (peek().type == TokenType::IDENTIFIER && peek().value == "as"))) {
        advance();
        Token varTok = consume(TokenType::IDENTIFIER, "Expected variable name after 'as'");
        if (varTok.type == TokenType::INVALID) return nullptr;
        lineVar = varTok.value;
    }
    
    // Consume colon if present
    if (!isAtEnd() && peek().type == TokenType::COLON) advance();
    
    // Parse the loop body as a block
    std::vector<std::shared_ptr<ASTNode>> bodyStmts;
    while (!isAtEnd()) {
        // Stop on dedent indicators or explicit end markers
        if (peek().type == TokenType::END ||
            (peek().type == TokenType::IDENTIFIER && peek().value == "Done") ||
            (peek().type == TokenType::IDENTIFIER && peek().value == "End")) {
            advance();
            break;
        }
        auto stmt = parseStatement();
        if (stmt) bodyStmts.push_back(stmt);
        else break;
    }
    
    auto body = node<BlockStatement>(bodyStmts);
    return node<StreamReadLoop>(scribeTok.value, lineVar, body);
}

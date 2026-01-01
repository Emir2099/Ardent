#include "lexer.h"
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cctype>
#include <regex>

Lexer::Lexer(const std::string& input) : input(input), currentPos(0), 
    currentChar(input.empty() ? '\0' : input[0]) {
    objectExpectKey.clear();
}

void Lexer::advance() {
    currentPos++;
    if (currentPos < input.length()) {
        currentChar = input[currentPos];
    } else {
        currentChar = '\0'; // End of input
    }
}

bool Lexer::isAlpha(char c) {
    return std::isalpha(c) || c == '_';
}

bool Lexer::isDigit(char c) {
    return std::isdigit(c);
}

Token Lexer::parseLet() {
    // Accept variations in spacing and case for the proclamation header
    // Pattern: "let it be known" with one or more spaces between words, case-insensitive
    static const std::regex pattern("^let\\s+it\\s+be\\s+known", std::regex_constants::icase);
    std::smatch m;
    std::string remaining = input.substr(currentPos);
    if (std::regex_search(remaining, m, pattern)) {
        // Advance by matched length
        currentPos += static_cast<size_t>(m.length());
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::LET, "Let it be known");
    }

    std::cerr << "Error: Expected 'Let it be known' at position " << currentPos << std::endl;
    return Token(TokenType::ERROR, "Invalid LET syntax");
}

Token Lexer::parseNamed() {
    std::string phrase = "a number named";
    size_t phraseLen = phrase.length();

    if (input.substr(currentPos, phraseLen) == phrase) {
        currentPos += phraseLen;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::NAMED, phrase);
    }

    std::cerr << "Error: Expected 'a number named' at position " << currentPos << std::endl;
    return Token(TokenType::ERROR, "Invalid NAMED syntax");
}

Token Lexer::parseIsOf() {
    std::string phrase = "is of";
    size_t phraseLen = phrase.length();

    if (input.substr(currentPos, phraseLen) == phrase) {
        currentPos += phraseLen;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::IS_OF, phrase);
    }

    std::cerr << "Error: Expected 'is of' at position " << currentPos << std::endl;
    return Token(TokenType::ERROR, "Invalid IS_OF syntax");
}


Token Lexer::parseLetProclaimed() {
    std::string phrase = "let it be proclaimed";
    size_t len = phrase.length();
    if (input.substr(currentPos, len) == phrase) {
        currentPos += len;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::LET_PROCLAIMED, phrase);
    }
    return Token(TokenType::ERROR, "Invalid LET_PROCLAIMED");
}

Token Lexer::parseGenericLet() {
    // Consume leading 'let' (case-insensitive)
    size_t start = currentPos;
    auto ciEqual = [](char a, char b){ return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); };
    auto matchWord = [&](const char* word) -> bool {
        size_t i = 0;
        while (word[i] && currentPos + i < input.size() && ciEqual(input[currentPos + i], word[i])) {
            ++i;
        }
        if (word[i] == '\0') {
            currentPos += i;
            currentChar = (currentPos < input.length()? input[currentPos] : '\0');
            return true;
        }
        return false;
    };
    if (!matchWord("let")) { currentPos = start; currentChar = (currentPos < input.length()? input[currentPos] : '\0'); return Token(TokenType::ERROR, "Invalid LET rite syntax"); }
    // Do not consume further words; let parser handle 'the order/tome ...'
    return Token(TokenType::LET, "Let");
}

Token Lexer::parseDecreeElders() {
    std::string phrase = "By decree of the elders";
    size_t len = phrase.length();
    if (input.substr(currentPos, len) == phrase) {
        currentPos += len;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::SPELL_DEF, phrase);
    }
    return Token(TokenType::ERROR, "Invalid DECREE_ELDERS");
}

Token Lexer::parseSpellNamed() {
    std::string phrase = "spell named";
    size_t len = phrase.length();
    if (input.substr(currentPos, len) == phrase) {
        currentPos += len;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::SPELL_NAMED, phrase);
    }
    return Token(TokenType::ERROR, "Invalid SPELL_NAMED");
}

Token Lexer::parseIsCastUpon() {
    std::string phrase = "is cast upon";
    size_t len = phrase.length();
    if (input.substr(currentPos, len) == phrase) {
        currentPos += len;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::SPELL_CAST, phrase);
    }
    return Token(TokenType::ERROR, "Invalid CAST_UPON");
}

Token Lexer::parseSpellCall() {
    std::string phrase = "Invoke the spell";
    size_t len = phrase.length();
    if (input.substr(currentPos, len) == phrase) {
        currentPos += len;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::SPELL_CALL, phrase);
    }
    return Token(TokenType::ERROR, "Invalid SPELL_CALL");
}

// identifier parser with keywords
Token Lexer::parseIdentifier() {
    std::string identifier;
    while (isAlpha(currentChar) || isDigit(currentChar) || currentChar == '_') {
        identifier += currentChar;
        advance();
    }

    // Booleans
    if (identifier == "True" || identifier == "False") {
        return Token(TokenType::BOOLEAN, identifier);
    }

    if (identifier == "Should") return Token(TokenType::SHOULD, identifier);
    if (identifier == "Try") return Token(TokenType::TRY, identifier);
    if (identifier == "Catch") return Token(TokenType::CATCH, identifier);
    if (identifier == "Finally") return Token(TokenType::FINALLY, identifier);
    if (identifier == "fates") return Token(TokenType::FATES, identifier);
    if (identifier == "decree") return Token(TokenType::DECREE, identifier);
    if (identifier == "surpasseth") return Token(TokenType::SURPASSETH, identifier);
    if (identifier == "then") return Token(TokenType::THEN, identifier);
    if (identifier == "whisper") return Token(TokenType::WHISPER, identifier);
    if (identifier == "Else") return Token(TokenType::ELSE, identifier);
    if (identifier == "ascend") return Token(TokenType::ASCEND, identifier); 
    if (identifier == "descend") return Token(TokenType::DESCEND, identifier);
    if (identifier == "and") return Token(TokenType::AND, identifier);
    if (identifier == "or") return Token(TokenType::OR, identifier);
    if (identifier == "not") return Token(TokenType::NOT, identifier);
    if (identifier == "cast") return Token(TokenType::CAST, identifier);
    if (identifier == "as") return Token(TokenType::AS, identifier);
    if (identifier == "expand") return Token(TokenType::EXPAND, identifier);
    if (identifier == "amend") return Token(TokenType::AMEND, identifier);
    if (identifier == "remove") return Token(TokenType::REMOVE, identifier);
    if (identifier == "erase") return Token(TokenType::ERASE, identifier);

    
    return Token(TokenType::IDENTIFIER, identifier);
}

// Read a bare identifier lexeme without keyword mapping (used for tome keys)
std::string Lexer::readBareIdentifier() {
    std::string identifier;
    while (isAlpha(currentChar) || isDigit(currentChar) || currentChar == '_') {
        identifier += currentChar;
        advance();
    }
    return identifier;
}


// parseNumber() to handle negative numbers
Token Lexer::parseNumber() {
    std::string number;
    // Check for negative sign
    if (currentChar == '-') {
        number += currentChar;
        advance();
    }
    while (isDigit(currentChar)) {
        number += currentChar;
        advance();
    }
    return Token(TokenType::NUMBER, number);
}

Token Lexer::parseString() {
    advance(); // Skip opening quote
    std::string value;
    while (currentChar != '"' && currentChar != '\0') {
        value += currentChar;
        advance();
    }
    if (currentChar == '"') advance(); // Skip closing quote
    return Token(TokenType::STRING, value);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (currentChar != '\0') {
        if (std::isspace(currentChar)) {
            advance();
        }
        // Multi-word phrases
        else if (std::regex_search(input.substr(currentPos), std::regex("^let\\s+it\\s+be\\s+known", std::regex_constants::icase))) {
            tokens.push_back(parseLet());
        }
        // Collection rite opening: 'Let the order' / 'Let the tome'
        else if (std::regex_search(input.substr(currentPos), std::regex("^let\\s+the\\s+(order|tome)", std::regex_constants::icase))) {
            tokens.push_back(parseGenericLet());
        }
        // Let it be proclaimed (print statement)
        else if (std::regex_search(input.substr(currentPos), std::regex("^let\\s+it\\s+be\\s+proclaimed", std::regex_constants::icase))) {
            // Skip past this phrase; it will be handled by the LET_PROCLAIMED pattern below
            // Actually handled further down in the tokenize loop
            tokens.push_back(Token(TokenType::LET_PROCLAIMED, "Let it be proclaimed"));
            currentPos += 20; // "Let it be proclaimed"
            while (currentPos < input.length() && std::isspace(input[currentPos])) currentPos++;
            if (currentPos < input.length() && input[currentPos] == ':') currentPos++; // consume optional ':'
            while (currentPos < input.length() && std::isspace(input[currentPos])) currentPos++;
            currentChar = (currentPos < input.length() ? input[currentPos] : '\0');
        }
        // 2.4 Stream/scribe declaration: 'Let a scribe <name> be opened upon <path>'
        else if (std::regex_search(input.substr(currentPos), std::regex("^let\\s+a\\s+scribe", std::regex_constants::icase))) {
            tokens.push_back(Token(TokenType::SCRIBE, "Let a scribe"));
            currentPos += 12; // "Let a scribe"
            while (currentPos < input.length() && std::isspace(input[currentPos])) currentPos++;
            currentChar = (currentPos < input.length() ? input[currentPos] : '\0');
        }
        // 2.2 Short-form: 'let <identifier>' for typed/untyped variable declarations
        // Must NOT match "let it" or "let the" or "let a scribe" (those are handled above)
        else if (std::regex_search(input.substr(currentPos), std::regex("^let\\s+(?!it\\s|the\\s|a\\s+scribe)[a-zA-Z_]", std::regex_constants::icase))) {
            // Just consume "let" and return LET token; identifier will be lexed separately
            currentPos += 3; // "let"
            while (currentPos < input.length() && std::isspace(input[currentPos])) currentPos++;
            currentChar = (currentPos < input.length() ? input[currentPos] : '\0');
            tokens.push_back(Token(TokenType::LET, "let"));
        }
        else if (input.substr(currentPos, 14) == "a number named") {
            tokens.push_back(parseNamed());
        }
        else if (input.substr(currentPos, 14) == "a phrase named") {
            // Treat phrase declarations similar to number declarations at lexing stage
            tokens.push_back(Token(TokenType::NAMED, "a phrase named"));
            currentPos += 14;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 13) == "a truth named") {
            // Treat boolean declarations similar to number/string declarations at lexing stage
            tokens.push_back(Token(TokenType::NAMED, "a truth named"));
            currentPos += 13;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 14) == "an order named") {
            tokens.push_back(Token(TokenType::NAMED, "an order named"));
            currentPos += 14;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 12) == "a tome named") {
            tokens.push_back(Token(TokenType::NAMED, "a tome named"));
            currentPos += 12;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 5) == "is of") {
            tokens.push_back(parseIsOf());
        }
        else if (input.substr(currentPos, 11) == "is equal to") {
            tokens.push_back(Token(TokenType::EQUAL, "is equal to"));
            currentPos += 11;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 6) == "is not") {
            tokens.push_back(Token(TokenType::NOT_EQUAL, "is not"));
            currentPos += 6;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 15) == "is greater than") {
            tokens.push_back(Token(TokenType::GREATER, "is greater than"));
            currentPos += 15;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 14) == "is lesser than") {
            tokens.push_back(Token(TokenType::LESSER, "is lesser than"));
            currentPos += 14;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 23) == "By decree of the elders") {
            tokens.push_back(parseDecreeElders());
        }
        else if (input.substr(currentPos, 11) == "spell named") {
            tokens.push_back(parseSpellNamed());
        }
        else if (input.substr(currentPos, 12) == "is cast upon") {
            tokens.push_back(parseIsCastUpon());
        }
        else if (input.substr(currentPos, 16) == "Invoke the spell") {
            tokens.push_back(parseSpellCall());
        }
        else if (input.substr(currentPos, 17) == "Invoke the spirit") {
            // Native invocation: "Invoke the spirit ..."
            tokens.push_back(Token(TokenType::NATIVE_CALL, "Invoke the spirit"));
            currentPos += 17;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 8) == "Inscribe") {
            tokens.push_back(Token(TokenType::INSCRIBE, "Inscribe"));
            currentPos += 8;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 4) == "Etch") {
            tokens.push_back(Token(TokenType::ETCH, "Etch"));
            currentPos += 4;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 12) == "reading from") {
            tokens.push_back(Token(TokenType::READING_FROM, "reading from"));
            currentPos += 12;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 19) == "Banish the scroll") {
            tokens.push_back(Token(TokenType::BANISH, "Banish the scroll"));
            currentPos += 19;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 6) == "Banish") {
            tokens.push_back(Token(TokenType::BANISH, "Banish"));
            currentPos += 6;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        // Async / Streams (Ardent 2.4)
        else if (input.substr(currentPos, 18) == "Await the omen of") {
            tokens.push_back(Token(TokenType::AWAIT, "Await the omen of"));
            currentPos += 18;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 5) == "Await") {
            tokens.push_back(Token(TokenType::AWAIT, "Await"));
            currentPos += 5;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 15) == "Write the verse") {
            tokens.push_back(Token(TokenType::WRITE_INTO, "Write the verse"));
            currentPos += 15;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 16) == "Close the scribe") {
            tokens.push_back(Token(TokenType::CLOSE, "Close the scribe"));
            currentPos += 16;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 16) == "Read from scribe") {
            tokens.push_back(Token(TokenType::READ_FROM_STREAM, "Read from scribe"));
            currentPos += 16;
            if (currentPos < input.length()) currentChar = input[currentPos]; else currentChar = '\0';
        }
        else if (input.substr(currentPos, 19) == "From the scroll of") {
            tokens.push_back(Token(TokenType::FROM_SCROLL, "From the scroll of"));
            currentPos += 19;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 18) == "draw all knowledge") {
            tokens.push_back(Token(TokenType::DRAW_ALL_KNOWLEDGE, "draw all knowledge"));
            currentPos += 18;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 4) == "take") {
            tokens.push_back(Token(TokenType::TAKE, "take"));
            currentPos += 4;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 18) == "Unfurl the scroll") {
            tokens.push_back(Token(TokenType::UNFURL_SCROLL, "Unfurl the scroll"));
            currentPos += 18;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 17) == "And let it return") {
            tokens.push_back(Token(TokenType::RETURN, "And let it return"));
            currentPos += 17;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 5) == "known") {
            // match 'known as'
            if (input.substr(currentPos, 8) == "known as") {
                tokens.push_back(Token(TokenType::KNOWN_AS, "known as"));
                currentPos += 8; currentChar = (currentPos < input.length()? input[currentPos] : '\0');
            }
        }
        else if (input.substr(currentPos, 4) == "upon") {
            tokens.push_back(Token(TokenType::UPON, "upon"));
            currentPos += 4; currentChar = (currentPos < input.length()? input[currentPos] : '\0');
        }
        else if (input.substr(currentPos, 21) == "Let it be proclaimed:") {
            tokens.push_back(Token(TokenType::LET_PROCLAIMED, "Let it be proclaimed"));
            currentPos += 21;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 20) == "Let it be proclaimed") {
            tokens.push_back(Token(TokenType::LET_PROCLAIMED, "Let it be proclaimed"));
            currentPos += 20;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 21) == "let it be proclaimed:") {
            tokens.push_back(Token(TokenType::LET_PROCLAIMED, "let it be proclaimed"));
            currentPos += 21;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        else if (input.substr(currentPos, 20) == "let it be proclaimed") {
            tokens.push_back(parseLetProclaimed());
        }
        else if (input.substr(currentPos, 24) == "Whilst the sun doth rise") {
            tokens.push_back(Token(TokenType::WHILST, "Whilst the sun doth rise"));
            currentPos += 24;
            if (currentPos < input.length())
                currentChar = input[currentPos];
            else
                currentChar = '\0';
        } 
        else if (input.substr(currentPos, 15) == "remaineth below") {
            tokens.push_back(Token(TokenType::REMAINETH, "remaineth below"));
            currentPos += 15;
            if (currentPos < input.length())
                currentChar = input[currentPos];
            else
                currentChar = '\0';
        } 
        else if (input.substr(currentPos, 30) == "so shall these words be spoken") {
            tokens.push_back(Token(TokenType::SPOKEN, "so shall these words be spoken"));
            currentPos += 30;
            if (currentPos < input.length())
                currentChar = input[currentPos];
            else
                currentChar = '\0';
        } 
        else if (input.substr(currentPos, 3) == "For") {
            tokens.push_back(Token(TokenType::FOR, "For"));
            currentPos += 3;
            if (currentPos < input.length())
                currentChar = input[currentPos];
            else
                currentChar = '\0';
        }
        else if (input.substr(currentPos, 22) == "Do as the fates decree") {
            tokens.push_back(Token(TokenType::DO_FATES, "Do as the fates decree"));
            currentPos += 22;
            if (currentPos < input.length())
                currentChar = input[currentPos];
            else
                currentChar = '\0';
        }
        else if (input.substr(currentPos, 5) == "Until") {
            tokens.push_back(Token(TokenType::UNTIL, "Until"));
            currentPos += 5;
            if (currentPos < input.length()) currentChar = input[currentPos];
            else currentChar = '\0';
        }
        // Single characters
        else if (currentChar == '"') {
            tokens.push_back(parseString());
        }
        else if (currentChar == '[') {
            tokens.push_back(Token(TokenType::LBRACKET, "["));
            advance();
        }
        else if (currentChar == ']') {
            tokens.push_back(Token(TokenType::RBRACKET, "]"));
            advance();
        }
        else if (currentChar == '{') {
            tokens.push_back(Token(TokenType::LBRACE, "{"));
            advance();
            // Enter object literal: expect a key next
            objectExpectKey.push_back(true);
        }
        else if (currentChar == '}') {
            tokens.push_back(Token(TokenType::RBRACE, "}"));
            advance();
            // Exit object literal scope if present
            if (!objectExpectKey.empty()) objectExpectKey.pop_back();
        }
        else if (currentChar == ',') {
            tokens.push_back(Token(TokenType::COMMA, ","));
            advance();
            // After comma inside object, expect another key
            if (!objectExpectKey.empty()) objectExpectKey.back() = true;
        }
        else if (currentChar == ':') {
            tokens.push_back(Token(TokenType::COLON, ":"));
            advance();
            // After colon inside object, we are parsing a value
            if (!objectExpectKey.empty()) objectExpectKey.back() = false;
        }
        else if (currentChar == '.') {
            // Only treat '.' as field-access when followed by an identifier start; otherwise ignore as punctuation
            if (isAlpha(peekNextChar())) {
                tokens.push_back(Token(TokenType::DOT, "."));
            }
            advance();
        }
        // Check for numbers (including negative)
else if (isDigit(currentChar) || (currentChar == '-' && isDigit(peekNextChar()))) {
    tokens.push_back(parseNumber());
}
        else if (currentChar == '+' || currentChar == '-' ||
                 currentChar == '*' || currentChar == '/' ||
                 currentChar == '%' || currentChar == '=') {
            std::string op(1, currentChar);
            tokens.push_back(Token(TokenType::OPERATOR, op));
            advance();
        }
        else if (isAlpha(currentChar)) {
            // If inside an object and expecting a key, convert bare identifier to STRING
            if (!objectExpectKey.empty() && objectExpectKey.back()) {
                auto key = readBareIdentifier();
                tokens.push_back(Token(TokenType::STRING, key));
            } else {
                tokens.push_back(parseIdentifier());
            }
        }
        else if (isDigit(currentChar)) {
            tokens.push_back(parseNumber());
        }
        else {
            advance(); // Skip unrecognized characters
        }
    }
    return tokens;
}

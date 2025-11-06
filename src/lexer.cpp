#include "lexer.h"
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cctype>
#include <regex>

Lexer::Lexer(const std::string& input) : input(input), currentPos(0), 
    currentChar(input.empty() ? '\0' : input[0]) {}

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

Token Lexer::parseDecreeElders() {
    std::string phrase = "By decree of the elders";
    size_t len = phrase.length();
    if (input.substr(currentPos, len) == phrase) {
        currentPos += len;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::DECREE_ELDERS, phrase);
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
        return Token(TokenType::CAST_UPON, phrase);
    }
    return Token(TokenType::ERROR, "Invalid CAST_UPON");
}

// identifier parser with keywords
Token Lexer::parseIdentifier() {
    std::string identifier;
    while (isAlpha(currentChar) || isDigit(currentChar) || currentChar == '_') {
        identifier += currentChar;
        advance();
    }

    if (identifier == "Should") return Token(TokenType::SHOULD, identifier);
    if (identifier == "fates") return Token(TokenType::FATES, identifier);
    if (identifier == "decree") return Token(TokenType::DECREE, identifier);
    if (identifier == "surpasseth") return Token(TokenType::SURPASSETH, identifier);
    if (identifier == "then") return Token(TokenType::THEN, identifier);
    if (identifier == "whisper") return Token(TokenType::WHISPER, identifier);
    if (identifier == "Else") return Token(TokenType::ELSE, identifier);
    if (identifier == "ascend") return Token(TokenType::ASCEND, identifier); 
    if (identifier == "descend") return Token(TokenType::DESCEND, identifier);

    
    return Token(TokenType::IDENTIFIER, identifier);
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
        else if (input.substr(currentPos, 5) == "is of") {
            tokens.push_back(parseIsOf());
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
            tokens.push_back(parseIdentifier());
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

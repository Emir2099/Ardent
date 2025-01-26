#include "lexer.h"
#include <iostream>
#include <vector>
#include <string>
#include <cctype>
#include <regex>

Lexer::Lexer(const std::string& input) : input(input), currentPos(0), currentChar(input[0]) {}

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
    std::string phrase = "Let it be known";
    size_t phraseLen = phrase.length();

    // Remove the "-1" from substr check
    if (input.substr(currentPos, phraseLen) == phrase) {
        currentPos += phraseLen;
        if (currentPos < input.length()) currentChar = input[currentPos];
        else currentChar = '\0';
        return Token(TokenType::LET, phrase);
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

Token Lexer::parseIdentifier() {
    std::string identifier = "";
    while (isAlpha(currentChar) || isDigit(currentChar) || currentChar == '_') {
        identifier += currentChar;
        advance();
    }
    return Token(TokenType::IDENTIFIER, identifier);
}

Token Lexer::parseNumber() {
    std::string number = "";
    while (isDigit(currentChar)) {
        number += currentChar;
        advance();
    }
    return Token(TokenType::NUMBER, number);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (currentChar != '\0') {
        if (std::isspace(currentChar)) {
            advance();  // Skip whitespace
        } 
        // Check for multi-word phrases first
        else if (input.substr(currentPos, 15) == "Let it be known") {
            tokens.push_back(parseLet());
        } 
        else if (input.substr(currentPos, 14) == "a number named") {
            tokens.push_back(parseNamed());
        } 
        else if (input.substr(currentPos, 5) == "is of") {
            tokens.push_back(parseIsOf());
        } 
        // Process individual words
        else if (isAlpha(currentChar)) {
            tokens.push_back(parseIdentifier());
        } 
        else if (isDigit(currentChar)) {
            tokens.push_back(parseNumber());
        } 
        else if (currentChar == ',' || currentChar == '.') {
            advance();  // Ignore punctuation
        } 
        else {
            std::cerr << "Skipping unexpected character: " << currentChar << std::endl;
            advance();
        }
    }

    return tokens;
}
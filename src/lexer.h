#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include "token.h"

class Lexer {
private:
    std::string input;
    size_t currentPos;         // Current position in input
    char currentChar;          // Current character being analyzed

    // Internal helper methods
    void advance();            // Advances to the next character
    bool isAlpha(char c);      // Check if character is alphabetic
    bool isDigit(char c);      // Check if character is a digit
    Token parseLet();          // Parse 'Let it be known'
    Token parseNamed();        // Parse 'a number named'
    Token parseIsOf();         // Parse 'is of'
    Token parseIdentifier();   // Parse identifiers
    Token parseNumber();       // Parse numbers

public:
    Lexer(const std::string& input);  // Constructor declaration
    std::vector<Token> tokenize();    // Tokenize the input string
};

#endif
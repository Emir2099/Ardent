#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include "token.h"

class Lexer {
private:
    std::string input;
    size_t currentPos;
    char currentChar;

    void advance();
    bool isAlpha(char c);
    bool isDigit(char c);
    Token parseLet();
    Token parseNamed();
    Token parseIsOf();
    Token parseIdentifier();
    Token parseNumber();
    Token parseString();
    Token parseDecreeElders();
    Token parseSpellNamed();
    Token parseIsCastUpon();
    Token parseLetProclaimed();
    
public:
    Lexer(const std::string& input);
    std::vector<Token> tokenize();
};

#endif
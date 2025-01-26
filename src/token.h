#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <vector>

enum class TokenType {
    LET, ERROR, IDENTIFIER, NUMBER, STRING, IF, THEN, ELSE, OPERATOR, END, INVALID,
    NAMED,   // Added custom token type for 'named'
    IS_OF    // Added custom token type for 'is of'
};

// Convert TokenType to a string for printing
inline std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::LET: return "LET";
        case TokenType::NAMED: return "NAMED";
        case TokenType::IS_OF: return "IS_OF";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::IF: return "IF";
        case TokenType::THEN: return "THEN";
        case TokenType::ELSE: return "ELSE";
        case TokenType::OPERATOR: return "OPERATOR";
        case TokenType::END: return "END";
        case TokenType::INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}

struct Token {
    TokenType type;
    std::string value;

    Token(TokenType type, const std::string& value) : type(type), value(value) {}

    TokenType getType() const {
        return type;
    }

    std::string getValue() const {
        return value;
    }
};

#endif
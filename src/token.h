#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <vector>

enum class TokenType {
    LET, ERROR, IDENTIFIER, NUMBER, STRING, OPERATOR, 
    NAMED, IS_OF, SHOULD, FATES, DECREE, SURPASSETH, 
    THEN, WHISPER, DECREE_ELDERS, SPELL_NAMED, CAST_UPON, 
    COMPARISON_OP, ELSE, LET_PROCLAIMED, INVALID, END, WHILST, REMAINETH, SPOKEN, ASCEND,DESCEND,AND_WITH_EACH_DAWN,
    FOR, DO_FATES, UNTIL, BOOLEAN, AND, OR, NOT
};

inline std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::LET: return "LET";
        case TokenType::NAMED: return "NAMED";
        case TokenType::IS_OF: return "IS_OF";
        case TokenType::SHOULD: return "SHOULD";
        case TokenType::FATES: return "FATES";
        case TokenType::DECREE: return "DECREE";
        case TokenType::SURPASSETH: return "SURPASSETH";
        case TokenType::THEN: return "THEN";
        case TokenType::WHISPER: return "WHISPER";
        case TokenType::DECREE_ELDERS: return "DECREE_ELDERS";
        case TokenType::SPELL_NAMED: return "SPELL_NAMED";
        case TokenType::CAST_UPON: return "CAST_UPON";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::AND_WITH_EACH_DAWN: return "AND_WITH_EACH_DAWN";
        case TokenType::OPERATOR: return "OPERATOR";
        case TokenType::COMPARISON_OP: return "COMPARISON_OP";
        case TokenType::ELSE: return "ELSE";
        case TokenType::LET_PROCLAIMED: return "LET_PROCLAIMED";
        case TokenType::WHILST: return "WHILST";
        case TokenType::REMAINETH: return "REMAINETH";
        case TokenType::SPOKEN: return "SPOKEN";
        case TokenType::ASCEND: return "ASCEND";
        case TokenType::DESCEND: return "DESCEND";
        case TokenType::FOR: return "FOR";
        case TokenType::DO_FATES: return "DO_FATES";
        case TokenType::UNTIL: return "UNTIL";
        case TokenType::BOOLEAN: return "BOOLEAN";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        default: return "UNKNOWN";
    }
}

struct Token {
    TokenType type;
    std::string value;
    Token(TokenType type, const std::string& value) : type(type), value(value) {}
    TokenType getType() const { return type; }
    std::string getValue() const { return value; }
};

#endif
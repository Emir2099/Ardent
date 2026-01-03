#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <vector>

enum class TokenType {
    LET, ERROR, IDENTIFIER, NUMBER, STRING, OPERATOR, 
    NAMED, IS_OF, SHOULD, FATES, DECREE, SURPASSETH, 
    THEN, WHISPER, DECREE_ELDERS, SPELL_NAMED, CAST_UPON, SPELL_DEF, SPELL_CAST, SPELL_CALL, UPON, KNOWN_AS,
    COMPARISON_OP, ELSE, LET_PROCLAIMED, INVALID, END, WHILST, REMAINETH, SPOKEN, ASCEND,DESCEND,AND_WITH_EACH_DAWN,
    FOR, DO_FATES, UNTIL, BOOLEAN, AND, OR, NOT, EQUAL, NOT_EQUAL, GREATER, LESSER, GREATER_EQUAL, LESSER_EQUAL, CAST, AS,
    LBRACKET, RBRACKET, LBRACE, RBRACE, COMMA, COLON, DOT,
    EXPAND, AMEND, REMOVE, ERASE,
    RETURN,
    FROM_SCROLL, DRAW_ALL_KNOWLEDGE, TAKE, UNFURL_SCROLL,
    NATIVE_CALL,
    TRY, CATCH, FINALLY, CURSE, OMEN,
    INSCRIBE, ETCH, READING_FROM, BANISH,
    // Async / Streams (Ardent 2.4)
    AWAIT, SCRIBE, OPENED, WRITE_INTO, CLOSE, READ_FROM_STREAM,
    // Collection iteration & operations (Ardent 3.1)
    EACH, ABIDETH, IN, WHERE, TRANSFORMED, BE,
    // Comments (Ardent 3.2)
    COMMENT, DOC_COMMENT,
    // Block control flow (Ardent 3.3)
    OTHERWISE, BECOME, CEASE, DONE, CONTINUE
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
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::LESSER: return "LESSER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::LESSER_EQUAL: return "LESSER_EQUAL";
        case TokenType::CAST: return "CAST";
        case TokenType::AS: return "AS";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::COMMA: return "COMMA";
        case TokenType::COLON: return "COLON";
        case TokenType::DOT: return "DOT";
        case TokenType::EXPAND: return "EXPAND";
        case TokenType::AMEND: return "AMEND";
        case TokenType::REMOVE: return "REMOVE";
        case TokenType::ERASE: return "ERASE";
    case TokenType::SPELL_DEF: return "SPELL_DEF";
    case TokenType::SPELL_CAST: return "SPELL_CAST";
    case TokenType::SPELL_CALL: return "SPELL_CALL";
    case TokenType::UPON: return "UPON";
    case TokenType::KNOWN_AS: return "KNOWN_AS";
    case TokenType::RETURN: return "RETURN";
        case TokenType::FROM_SCROLL: return "FROM_SCROLL";
        case TokenType::DRAW_ALL_KNOWLEDGE: return "DRAW_ALL_KNOWLEDGE";
        case TokenType::TAKE: return "TAKE";
        case TokenType::UNFURL_SCROLL: return "UNFURL_SCROLL";
        case TokenType::NATIVE_CALL: return "NATIVE_CALL";
        case TokenType::TRY: return "TRY";
        case TokenType::CATCH: return "CATCH";
        case TokenType::FINALLY: return "FINALLY";
        case TokenType::CURSE: return "CURSE";
        case TokenType::OMEN: return "OMEN";
        case TokenType::INSCRIBE: return "INSCRIBE";
        case TokenType::ETCH: return "ETCH";
        case TokenType::READING_FROM: return "READING_FROM";
        case TokenType::BANISH: return "BANISH";
        // Async / Streams (2.4)
        case TokenType::AWAIT: return "AWAIT";
        case TokenType::SCRIBE: return "SCRIBE";
        case TokenType::OPENED: return "OPENED";
        case TokenType::WRITE_INTO: return "WRITE_INTO";
        case TokenType::CLOSE: return "CLOSE";
        case TokenType::READ_FROM_STREAM: return "READ_FROM_STREAM";
        // Collection iteration & operations (3.1)
        case TokenType::EACH: return "EACH";
        case TokenType::ABIDETH: return "ABIDETH";
        case TokenType::IN: return "IN";
        case TokenType::WHERE: return "WHERE";
        case TokenType::TRANSFORMED: return "TRANSFORMED";
        case TokenType::BE: return "BE";
        // Comments (3.2)
        case TokenType::COMMENT: return "COMMENT";
        case TokenType::DOC_COMMENT: return "DOC_COMMENT";
        // Block control flow (3.3)
        case TokenType::OTHERWISE: return "OTHERWISE";
        case TokenType::BECOME: return "BECOME";
        case TokenType::CEASE: return "CEASE";
        case TokenType::DONE: return "DONE";
        case TokenType::CONTINUE: return "CONTINUE";
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
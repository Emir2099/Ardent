#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include <memory>
#include <vector>

int main() {
    std::string input = R"(
        Let it be known throughout the land, a number named count is of 1 winters.
        Whilst the sun doth rise, count remaineth below 5,
        so shall these words be spoken:
            "Lo, count is" + count
        And with each dawn, let count ascend 1.
    )";

    Lexer lexer(input);
    std::vector<Token> tokens = lexer.tokenize();

    std::cout << "=== Tokens Generated ===" << std::endl;
    for (const auto& token : tokens) {
        std::cout << "Token: " << token.value << ", Type: " << tokenTypeToString(token.type) << std::endl;
    }

    Parser parser(tokens);
    auto ast = parser.parse();
if (!ast) {
    std::cerr << "Error: Parser returned NULL AST!" << std::endl;
    return 1;
}

std::cout << "=== AST Debug Output ===" << std::endl;
std::cout << typeid(*ast).name() << std::endl;  // Print AST node type

std::cout << "Parsing complete!" << std::endl;


    Interpreter interpreter;
    interpreter.execute(ast);

    return 0;
}




        // Let it be known throughout the land, a number named age is of 12 winters.
        // Should the fates decree that age surpasseth 18, 
        // then let it be proclaimed: "Thou art of age."
        // Else whisper: "Nay."
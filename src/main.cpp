#include <iostream>
#include "lexer.h"
#include "parser.h"
#include <vector>
#include "token.h"

int main() {
    std::string input = R"(
        Should the fates decree that age surpasseth 18, 
        then let it be proclaimed: "Thou art of age."
        Else whisper: "Nay."
        By decree of the elders, a spell named greet is cast upon name:
            "Hail " + name + "!"
    )";

    Lexer lexer(input);
    std::vector<Token> tokens = lexer.tokenize();

    Parser parser(tokens);
    auto ast = parser.parse();

    //     auto tokens = lexer.tokenize();

    // for (const auto& token : tokens) {
    //     std::cout << token.getValue() << " : " 
    //               << tokenTypeToString(token.getType()) << "\n";


    std::cout << "Parsing complete!" << std::endl;
    return 0;
}

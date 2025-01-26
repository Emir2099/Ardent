#include <iostream>
#include <vector>
#include "token.h"
#include "lexer.h"

int main() {
    std::string input = "Let it be known throughout the land, a number named age is of 25 winters.";
    
    Lexer lexer(input);
    std::vector<Token> tokens = lexer.tokenize();

    for (const auto& token : tokens) {
        std::cout << "Token: " << token.getValue() << ", Type: " << tokenTypeToString(token.getType()) << std::endl;
    }

    return 0;
}
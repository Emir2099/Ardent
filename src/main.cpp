#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include <memory>
#include <vector>

int main() {
    std::string input =  R"(
    Let it be known throughout the land, a number named ct is of 0 winters.  
    Let it be known throughout the land, a number named count is of -3 winters.
    Let it be known throughout the land, a phrase named greeting is of "Hello, world!".  
    Let it be proclaimed: greeting + " How art thou?"
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




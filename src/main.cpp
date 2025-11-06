#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include <memory>
#include <vector>

int main() {
    std::string input =  R"(
    # Previous demo code
    Let it be known throughout the land, a number named ct is of 0 winters.  
    Let it be known throughout the land, a number named count is of -3 winters.
    Let it be known throughout the land, a phrase named greeting is of "Hello, world!".  
    Let it be proclaimed: greeting + " How art thou?"
   
    Let it be known throughout the land, a truth named flag is of True.
    Let it be proclaimed: True
    Let it be proclaimed: flag
    Let it be known throughout the land, a truth named off is of False.
    Let it be proclaimed: off

    Let it be known throughout the land, a truth named brave is of True.
    Let it be known throughout the land, a truth named strong is of False.
    Should the fates decree brave and strong then Let it be proclaimed: "and-ok" Else whisper "and-nay"
    Should the fates decree brave or strong then Let it be proclaimed: "or-ok" Else whisper "or-nay"
    Should the fates decree not brave then Let it be proclaimed: "not-yes" Else whisper "not-no"
    Should the fates decree brave and not strong or False then Let it be proclaimed: "prec-pass" Else whisper "prec-fail"

    Let it be known throughout the land, a number named age is of 18 winters.
    Should the fates decree that age is equal to 18 then Let it be proclaimed: "Aye!" Else whisper "Nay!"
    Let it be known throughout the land, a number named cnt is of 0 winters.
    Should the fates decree that cnt is not 0 then Let it be proclaimed: "Not zero!" Else whisper "Zero!"
    Let it be known throughout the land, a number named x is of 7 winters.
    Should the fates decree that x is greater than 3 then Let it be proclaimed: "x>3" Else whisper "x<=3"
    Should the fates decree that x is lesser than 10 then Let it be proclaimed: "x<10" Else whisper "x>=10"


    Let it be known throughout the land, a number named n is of 25 winters.
    Let it be known throughout the land, a phrase named msg is of "The number is ".
    Let it be proclaimed: msg + cast n as phrase

    Let it be known throughout the land, a truth named nonzero is of cast n as truth.
    Let it be proclaimed: nonzero

    Let it be proclaimed: cast True as number

    Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].
    Let it be proclaimed: heroes[1]
    Let it be proclaimed: heroes[4]
    Let it be known throughout the land, a tome named hero is of {"name": "Aragorn", "title": "King of Gondor"}.
    Let it be proclaimed: hero["title"]
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




#include <iostream>
#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "token.h"
int main(){
    std::string code = R"(By decree of the elders, a spell named forge is cast upon a traveler known as who:
Let it be known throughout the land, a phrase named temp is of "Secret".
Let it be proclaimed: "Crafting for " + who
Invoke the spell forge upon "Rune"
)";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    std::cout << "--- TOKENS ---\n";
    for(auto &t: tokens){ std::cout << tokenTypeToString(t.type) << " | " << t.value << "\n"; }
    Parser parser(tokens);
    auto ast = parser.parse();
    if(!ast){ std::cerr << "Parse failed" << std::endl; return 1; }
    std::cout << "--- EXECUTION OUTPUT ---\n";
    Interpreter interp;
    interp.execute(ast);
    return 0;
}

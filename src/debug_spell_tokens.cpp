#include <iostream>
#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "token.h"
int main(){
    std::string code = R"(\
Let it be proclaimed: Invoke the spirit of math.add upon 2, 3\
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

#include <iostream>
#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "token.h"
int main(){
    std::string code = R"(\
Try:\
Try:\
Invoke the spirit of math.divide upon 1, 0\
Catch the curse as omen:\
Let it be proclaimed: "Inner: " + omen\
Catch the curse as outer:\
Let it be proclaimed: "Outer: " + outer\
)";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    std::cout << "--- TOKENS ---\n";
    for(auto &t: tokens){ std::cout << tokenTypeToString(t.type) << " | " << t.value << "\n"; }
    Parser parser(tokens);
    auto ast = parser.parse();
    if(!ast){ std::cerr << "Parse failed" << std::endl; return 1; }
    std::cout << "--- EXECUTION OUTPUT ---\n";
    // Inspect TryCatch
    if (auto block = std::dynamic_pointer_cast<BlockStatement>(ast)) {
        if (!block->statements.empty()) {
            if (auto tc = std::dynamic_pointer_cast<TryCatch>(block->statements[0])) {
                std::cout << "[DEBUG] TryCatch hasCatch=" << (tc->catchBlock?"yes":"no") << ", hasFinally=" << (tc->finallyBlock?"yes":"no") << ", catchVar='" << tc->catchVar << "'\n";
            }
        }
    }
    Interpreter interp;
    interp.execute(ast);
    return 0;
}

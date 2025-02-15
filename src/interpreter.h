#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <unordered_map>
#include "ast.h"

class Interpreter {
private:
    std::unordered_map<std::string, int> variables; // Stores variables and their values
    int evaluateExpr(std::shared_ptr<ASTNode> expr); 
    std::string evaluatePrintExpr(std::shared_ptr<ASTNode> expr);
public:
    void execute(std::shared_ptr<ASTNode> ast);
    void evaluateExpression(std::shared_ptr<ASTNode> expr);
    void assignVariable(std::string name, int value);
    int getVariable(std::string name);
    void executeWhileLoop(std::shared_ptr<WhileLoop> loop);
    void executeForLoop(std::shared_ptr<ForLoop> loop);

};

#endif

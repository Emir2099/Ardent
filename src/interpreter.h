#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <unordered_map>
#include <variant>
#include "ast.h"

class Interpreter {
private:
    // Allow integers and strings as variable values
    using Value = std::variant<int, std::string>;
    std::unordered_map<std::string, Value> variables; // Stores variables and their values
    int evaluateExpr(std::shared_ptr<ASTNode> expr); 
    std::string evaluatePrintExpr(std::shared_ptr<ASTNode> expr);
public:
    void execute(std::shared_ptr<ASTNode> ast);
    void evaluateExpression(std::shared_ptr<ASTNode> expr);
    void assignVariable(const std::string& name, int value);
    void assignVariable(const std::string& name, const std::string& value);
    int getIntVariable(const std::string& name);
    std::string getStringVariable(const std::string& name);
    void executeWhileLoop(std::shared_ptr<WhileLoop> loop);
    void executeForLoop(std::shared_ptr<ForLoop> loop);
    void executeDoWhileLoop(std::shared_ptr<DoWhileLoop> loop);

};

#endif

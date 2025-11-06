#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <unordered_map>
#include <variant>
#include <vector>
#include "ast.h"

class Interpreter {
public:
    // Allow integers, strings, and booleans as variable values
    using SimpleValue = std::variant<int, std::string, bool>;
    using Value = std::variant<int, std::string, bool, std::vector<SimpleValue>, std::unordered_map<std::string, SimpleValue>>;
private:
    std::unordered_map<std::string, Value> variables; // Stores variables and their values
    bool runtimeError = false; // flag to suppress output on runtime errors (e.g., bounds)
    int evaluateExpr(std::shared_ptr<ASTNode> expr); 
    std::string evaluatePrintExpr(std::shared_ptr<ASTNode> expr);
    Value evaluateValue(std::shared_ptr<ASTNode> expr);
public:
    void execute(std::shared_ptr<ASTNode> ast);
    void evaluateExpression(std::shared_ptr<ASTNode> expr);
    void assignVariable(const std::string& name, int value);
    void assignVariable(const std::string& name, const std::string& value);
    void assignVariable(const std::string& name, bool value);
    int getIntVariable(const std::string& name);
    std::string getStringVariable(const std::string& name);
    void executeWhileLoop(std::shared_ptr<WhileLoop> loop);
    void executeForLoop(std::shared_ptr<ForLoop> loop);
    void executeDoWhileLoop(std::shared_ptr<DoWhileLoop> loop);

};

#endif

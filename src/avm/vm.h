#ifndef ARDENT_AVM_VM_H
#define ARDENT_AVM_VM_H

#include <variant>
#include <vector>
#include <string>
#include <optional>
#include <iostream>
#include "opcode.h"
#include "bytecode.h"

namespace avm {

class VM {
public:
    struct Result {
        std::optional<Value> value; // final stack top if any
        bool ok{true};
    };

    // Clears all persisted variable slots
    void reset() { slots_.clear(); }

    Result run(const Chunk& chunk) {
        const auto& code = chunk.code;
        const auto& k = chunk.constants;
        size_t ip = 0;
        std::vector<Value> stack;
        // Use persistent variable slots across runs (REPL hot-reload)
        auto pop = [&]() {
            Value v = std::move(stack.back());
            stack.pop_back();
            return v;
        };
        auto push = [&](Value v) { stack.emplace_back(std::move(v)); };

        while (ip < code.size()) {
            OpCode op = static_cast<OpCode>(code[ip++]);
            switch (op) {
                case OpCode::OP_NOP: break;
                case OpCode::OP_JMP: {
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t off = read_u16(&code[ip]); ip += 2;
                    ip += off; // relative from end of operand
                    break;
                }
                case OpCode::OP_JMP_IF_FALSE: {
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t off = read_u16(&code[ip]); ip += 2;
                    if (stack.empty()) return {std::nullopt, false};
                    Value v = pop();
                    bool truth = false;
                    if (std::holds_alternative<bool>(v)) truth = std::get<bool>(v);
                    else if (std::holds_alternative<int>(v)) truth = (std::get<int>(v) != 0);
                    else if (std::holds_alternative<std::string>(v)) truth = !std::get<std::string>(v).empty();
                    if (!truth) ip += off;
                    break;
                }
                case OpCode::OP_PUSH_CONST: {
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t idx = read_u16(&code[ip]); ip += 2;
                    if (idx >= k.size()) return {std::nullopt, false};
                    push(k[idx]);
                    break;
                }
                case OpCode::OP_POP: {
                    if (stack.empty()) return {std::nullopt, false};
                    stack.pop_back();
                    break;
                }
                case OpCode::OP_LOAD: {
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t slot = read_u16(&code[ip]); ip += 2;
                    if (slot < slots_.size()) push(slots_[slot]);
                    else push(0); // default int 0
                    break;
                }
                case OpCode::OP_STORE: {
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t slot = read_u16(&code[ip]); ip += 2;
                    if (stack.empty()) return {std::nullopt, false};
                    Value v = pop();
                    if (slot >= slots_.size()) slots_.resize(slot + 1);
                    slots_[slot] = v;
                    break;
                }
                case OpCode::OP_NOT: {
                    if (stack.empty()) return {std::nullopt, false};
                    Value v = pop();
                    bool truth = false;
                    if (std::holds_alternative<bool>(v)) truth = std::get<bool>(v);
                    else if (std::holds_alternative<int>(v)) truth = (std::get<int>(v) != 0);
                    else if (std::holds_alternative<std::string>(v)) truth = !std::get<std::string>(v).empty();
                    push(!truth);
                    break;
                }
                case OpCode::OP_ADD: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                        push(std::get<int>(a) + std::get<int>(b));
                    } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                        push(std::get<std::string>(a) + std::get<std::string>(b));
                    } else {
                        // rudimentary: stringify other combos
                        std::string sa = std::holds_alternative<std::string>(a) ? std::get<std::string>(a)
                                              : (std::holds_alternative<int>(a) ? std::to_string(std::get<int>(a))
                                              : (std::get<bool>(a) ? "True" : "False"));
                        std::string sb = std::holds_alternative<std::string>(b) ? std::get<std::string>(b)
                                              : (std::holds_alternative<int>(b) ? std::to_string(std::get<int>(b))
                                              : (std::get<bool>(b) ? "True" : "False"));
                        push(sa + sb);
                    }
                    break;
                }
                case OpCode::OP_SUB: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                        push(std::get<int>(a) - std::get<int>(b));
                    } else return {std::nullopt, false};
                    break;
                }
                case OpCode::OP_MUL: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                        push(std::get<int>(a) * std::get<int>(b));
                    } else return {std::nullopt, false};
                    break;
                }
                case OpCode::OP_DIV: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                        int denom = std::get<int>(b);
                        if (denom == 0) return {std::nullopt, false};
                        push(std::get<int>(a) / denom);
                    } else return {std::nullopt, false};
                    break;
                }
                case OpCode::OP_AND: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    auto truthy = [](const Value& v){
                        if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
                        if (std::holds_alternative<int>(v)) return std::get<int>(v) != 0;
                        if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty();
                        return false;
                    };
                    push(truthy(a) && truthy(b));
                    break;
                }
                case OpCode::OP_OR: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    auto truthy = [](const Value& v){
                        if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
                        if (std::holds_alternative<int>(v)) return std::get<int>(v) != 0;
                        if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty();
                        return false;
                    };
                    push(truthy(a) || truthy(b));
                    break;
                }
                case OpCode::OP_PRINT: {
                    if (stack.empty()) return {std::nullopt, false};
                    const Value& v = stack.back();
                    if (std::holds_alternative<int>(v)) std::cout << std::get<int>(v) << "\n";
                    else if (std::holds_alternative<std::string>(v)) std::cout << std::get<std::string>(v) << "\n";
                    else std::cout << (std::get<bool>(v) ? "True" : "False") << "\n";
                    break;
                }
                case OpCode::OP_EQ: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    bool eq = false;
                    if (a.index() == b.index()) {
                        if (std::holds_alternative<int>(a)) eq = std::get<int>(a) == std::get<int>(b);
                        else if (std::holds_alternative<bool>(a)) eq = std::get<bool>(a) == std::get<bool>(b);
                        else eq = std::get<std::string>(a) == std::get<std::string>(b);
                    } else {
                        // Fallback: compare stringified values
                        auto toS = [](const Value& v){
                            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
                            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
                            return std::get<std::string>(v);
                        };
                        eq = toS(a) == toS(b);
                    }
                    push(eq);
                    break;
                }
                case OpCode::OP_NE: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    bool ne = false;
                    if (a.index() == b.index()) {
                        if (std::holds_alternative<int>(a)) ne = std::get<int>(a) != std::get<int>(b);
                        else if (std::holds_alternative<bool>(a)) ne = std::get<bool>(a) != std::get<bool>(b);
                        else ne = std::get<std::string>(a) != std::get<std::string>(b);
                    } else {
                        auto toS = [](const Value& v){
                            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
                            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
                            return std::get<std::string>(v);
                        };
                        ne = toS(a) != toS(b);
                    }
                    push(ne);
                    break;
                }
                case OpCode::OP_GT: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                        push(std::get<int>(a) > std::get<int>(b));
                    } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                        push(std::get<std::string>(a) > std::get<std::string>(b));
                    } else {
                        auto toS = [](const Value& v){
                            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
                            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
                            return std::get<std::string>(v);
                        };
                        push(toS(a) > toS(b));
                    }
                    break;
                }
                case OpCode::OP_LT: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                        push(std::get<int>(a) < std::get<int>(b));
                    } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                        push(std::get<std::string>(a) < std::get<std::string>(b));
                    } else {
                        auto toS = [](const Value& v){
                            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
                            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
                            return std::get<std::string>(v);
                        };
                        push(toS(a) < toS(b));
                    }
                    break;
                }
                case OpCode::OP_GE: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                        push(std::get<int>(a) >= std::get<int>(b));
                    } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                        push(std::get<std::string>(a) >= std::get<std::string>(b));
                    } else {
                        auto toS = [](const Value& v){
                            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
                            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
                            return std::get<std::string>(v);
                        };
                        push(toS(a) >= toS(b));
                    }
                    break;
                }
                case OpCode::OP_LE: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                        push(std::get<int>(a) <= std::get<int>(b));
                    } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                        push(std::get<std::string>(a) <= std::get<std::string>(b));
                    } else {
                        auto toS = [](const Value& v){
                            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
                            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
                            return std::get<std::string>(v);
                        };
                        push(toS(a) <= toS(b));
                    }
                    break;
                }
                case OpCode::OP_HALT: {
                    std::optional<Value> out;
                    if (!stack.empty()) out = stack.back();
                    return {out, true};
                }
                default:
                    return {std::nullopt, false};
            }
        }
        std::optional<Value> out;
        if (!stack.empty()) out = stack.back();
        return {out, true};
    }
private:
    std::vector<Value> slots_;
};

} 

#endif 

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

    Result run(const Chunk& chunk) {
        const auto& code = chunk.code;
        const auto& k = chunk.constants;
        size_t ip = 0;
        std::vector<Value> stack;
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
                case OpCode::OP_PRINT: {
                    if (stack.empty()) return {std::nullopt, false};
                    const Value& v = stack.back();
                    if (std::holds_alternative<int>(v)) std::cout << std::get<int>(v) << "\n";
                    else if (std::holds_alternative<std::string>(v)) std::cout << std::get<std::string>(v) << "\n";
                    else std::cout << (std::get<bool>(v) ? "True" : "False") << "\n";
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
};

} 

#endif 

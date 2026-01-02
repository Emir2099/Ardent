#ifndef ARDENT_AVM_VM_H
#define ARDENT_AVM_VM_H

#include <variant>
#include <vector>
#include <string>
#include <optional>
#include <iostream>
#include <unordered_map>
#include <functional>
#include "opcode.h"
#include "bytecode.h"

namespace avm {

// ============================================================================
// Function Table Entry for Cached Dispatch
// ============================================================================
struct FunctionEntry {
    uint16_t funcId;           // Function ID in bytecode
    size_t entryPoint;         // Cached entry point (bytecode offset)
    uint8_t arity;             // Number of arguments
    bool resolved{false};      // Has this entry been resolved?
};

// ============================================================================
// Call Site Cache for Inline Caching
// ============================================================================
struct CallSiteCache {
    uint16_t funcId{0xFFFF};   // Cached function ID (0xFFFF = uninitialized)
    size_t entryPoint{0};      // Cached entry point
    uint8_t arity{0};          // Cached arity
    uint32_t hitCount{0};      // Number of cache hits (for profiling)
};

class VM {
public:
    struct Result {
        std::optional<Value> value; // final stack top if any
        bool ok{true};
    };

    // Clears all persisted variable slots and caches
    void reset() {
        slots_.clear();
        callSiteCache_.clear();
        functionTable_.clear();
    }

    // Register a function in the function table
    void registerFunction(uint16_t funcId, size_t entryPoint, uint8_t arity) {
        FunctionEntry entry;
        entry.funcId = funcId;
        entry.entryPoint = entryPoint;
        entry.arity = arity;
        entry.resolved = true;
        functionTable_[funcId] = entry;
    }

    // Get cache statistics
    size_t getCacheHits() const {
        size_t total = 0;
        for (const auto& [offset, cache] : callSiteCache_) {
            total += cache.hitCount;
        }
        return total;
    }

    size_t getCacheMisses() const { return cacheMisses_; }

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
                    uint16_t rawOff = read_u16(&code[ip]); ip += 2;
                    int16_t off = static_cast<int16_t>(rawOff); // signed offset
                    ip = static_cast<size_t>(static_cast<ssize_t>(ip) + off);
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
                // ============================================================
                // Collection Operations (Ardent 3.1+)
                // ============================================================
                case OpCode::OP_MAKE_ORDER: {
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t count = read_u16(&code[ip]); ip += 2;
                    if (stack.size() < count) return {std::nullopt, false};
                    auto order = std::make_shared<VMOrder>();
                    order->elements.resize(count);
                    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
                        Value v = pop();
                        if (std::holds_alternative<int>(v)) order->elements[i] = std::get<int>(v);
                        else if (std::holds_alternative<std::string>(v)) order->elements[i] = std::get<std::string>(v);
                        else if (std::holds_alternative<bool>(v)) order->elements[i] = std::get<bool>(v);
                    }
                    push(order);
                    break;
                }
                case OpCode::OP_MAKE_TOME: {
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t count = read_u16(&code[ip]); ip += 2;
                    if (stack.size() < count * 2) return {std::nullopt, false};
                    auto tome = std::make_shared<VMTome>();
                    for (size_t i = 0; i < count; ++i) {
                        Value val = pop();
                        Value key = pop();
                        std::string keyStr;
                        if (std::holds_alternative<std::string>(key)) keyStr = std::get<std::string>(key);
                        else continue;
                        if (std::holds_alternative<int>(val)) tome->entries[keyStr] = std::get<int>(val);
                        else if (std::holds_alternative<std::string>(val)) tome->entries[keyStr] = std::get<std::string>(val);
                        else if (std::holds_alternative<bool>(val)) tome->entries[keyStr] = std::get<bool>(val);
                        tome->keyOrder.push_back(keyStr);
                    }
                    push(tome);
                    break;
                }
                case OpCode::OP_ORDER_GET: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value idxVal = pop();
                    Value orderVal = pop();
                    if (!std::holds_alternative<std::shared_ptr<VMOrder>>(orderVal)) return {std::nullopt, false};
                    if (!std::holds_alternative<int>(idxVal)) return {std::nullopt, false};
                    auto ord = std::get<std::shared_ptr<VMOrder>>(orderVal);
                    int idx = std::get<int>(idxVal);
                    if (idx < 0) idx += static_cast<int>(ord->elements.size());
                    if (idx < 0 || static_cast<size_t>(idx) >= ord->elements.size()) return {std::nullopt, false};
                    const auto& elem = ord->elements[idx];
                    if (std::holds_alternative<int>(elem)) push(std::get<int>(elem));
                    else if (std::holds_alternative<std::string>(elem)) push(std::get<std::string>(elem));
                    else push(std::get<bool>(elem));
                    break;
                }
                case OpCode::OP_ORDER_SET: {
                    if (stack.size() < 3) return {std::nullopt, false};
                    Value newVal = pop();
                    Value idxVal = pop();
                    Value orderVal = pop();
                    if (!std::holds_alternative<std::shared_ptr<VMOrder>>(orderVal)) return {std::nullopt, false};
                    if (!std::holds_alternative<int>(idxVal)) return {std::nullopt, false};
                    auto ord = std::get<std::shared_ptr<VMOrder>>(orderVal);
                    int idx = std::get<int>(idxVal);
                    if (idx < 0) idx += static_cast<int>(ord->elements.size());
                    if (idx < 0 || static_cast<size_t>(idx) >= ord->elements.size()) return {std::nullopt, false};
                    if (std::holds_alternative<int>(newVal)) ord->elements[idx] = std::get<int>(newVal);
                    else if (std::holds_alternative<std::string>(newVal)) ord->elements[idx] = std::get<std::string>(newVal);
                    else if (std::holds_alternative<bool>(newVal)) ord->elements[idx] = std::get<bool>(newVal);
                    push(ord);
                    break;
                }
                case OpCode::OP_ORDER_LEN: {
                    if (stack.empty()) return {std::nullopt, false};
                    Value orderVal = pop();
                    if (!std::holds_alternative<std::shared_ptr<VMOrder>>(orderVal)) return {std::nullopt, false};
                    push(static_cast<int>(std::get<std::shared_ptr<VMOrder>>(orderVal)->elements.size()));
                    break;
                }
                case OpCode::OP_ORDER_PUSH: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value newVal = pop();
                    Value orderVal = pop();
                    if (!std::holds_alternative<std::shared_ptr<VMOrder>>(orderVal)) return {std::nullopt, false};
                    auto ord = std::get<std::shared_ptr<VMOrder>>(orderVal);
                    // Create new order with appended value
                    auto newOrd = std::make_shared<VMOrder>();
                    newOrd->elements = ord->elements;
                    if (std::holds_alternative<int>(newVal)) newOrd->elements.push_back(std::get<int>(newVal));
                    else if (std::holds_alternative<std::string>(newVal)) newOrd->elements.push_back(std::get<std::string>(newVal));
                    else if (std::holds_alternative<bool>(newVal)) newOrd->elements.push_back(std::get<bool>(newVal));
                    push(newOrd);
                    break;
                }
                case OpCode::OP_TOME_GET: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value keyVal = pop();
                    Value tomeVal = pop();
                    if (!std::holds_alternative<std::shared_ptr<VMTome>>(tomeVal)) return {std::nullopt, false};
                    if (!std::holds_alternative<std::string>(keyVal)) return {std::nullopt, false};
                    auto tome = std::get<std::shared_ptr<VMTome>>(tomeVal);
                    const std::string& key = std::get<std::string>(keyVal);
                    auto it = tome->entries.find(key);
                    if (it == tome->entries.end()) { push(0); break; }
                    const auto& val = it->second;
                    if (std::holds_alternative<int>(val)) push(std::get<int>(val));
                    else if (std::holds_alternative<std::string>(val)) push(std::get<std::string>(val));
                    else push(std::get<bool>(val));
                    break;
                }
                case OpCode::OP_TOME_SET: {
                    if (stack.size() < 3) return {std::nullopt, false};
                    Value newVal = pop();
                    Value keyVal = pop();
                    Value tomeVal = pop();
                    if (!std::holds_alternative<std::shared_ptr<VMTome>>(tomeVal)) return {std::nullopt, false};
                    if (!std::holds_alternative<std::string>(keyVal)) return {std::nullopt, false};
                    auto tome = std::get<std::shared_ptr<VMTome>>(tomeVal);
                    const std::string& key = std::get<std::string>(keyVal);
                    if (tome->entries.find(key) == tome->entries.end()) tome->keyOrder.push_back(key);
                    if (std::holds_alternative<int>(newVal)) tome->entries[key] = std::get<int>(newVal);
                    else if (std::holds_alternative<std::string>(newVal)) tome->entries[key] = std::get<std::string>(newVal);
                    else if (std::holds_alternative<bool>(newVal)) tome->entries[key] = std::get<bool>(newVal);
                    push(tome);
                    break;
                }
                case OpCode::OP_TOME_HAS: {
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value keyVal = pop();
                    Value tomeVal = pop();
                    if (!std::holds_alternative<std::shared_ptr<VMTome>>(tomeVal)) return {std::nullopt, false};
                    if (!std::holds_alternative<std::string>(keyVal)) { push(false); break; }
                    auto tome = std::get<std::shared_ptr<VMTome>>(tomeVal);
                    push(tome->entries.find(std::get<std::string>(keyVal)) != tome->entries.end());
                    break;
                }
                case OpCode::OP_CONTAINS: {
                    // pop collection, pop needle, push bool
                    if (stack.size() < 2) return {std::nullopt, false};
                    Value collVal = pop();
                    Value needleVal = pop();
                    bool found = false;
                    if (std::holds_alternative<std::shared_ptr<VMOrder>>(collVal)) {
                        auto ord = std::get<std::shared_ptr<VMOrder>>(collVal);
                        for (const auto& elem : ord->elements) {
                            if (elem.index() == needleVal.index()) {
                                if (std::holds_alternative<int>(needleVal) && std::holds_alternative<int>(elem)) {
                                    if (std::get<int>(elem) == std::get<int>(needleVal)) { found = true; break; }
                                } else if (std::holds_alternative<std::string>(needleVal) && std::holds_alternative<std::string>(elem)) {
                                    if (std::get<std::string>(elem) == std::get<std::string>(needleVal)) { found = true; break; }
                                } else if (std::holds_alternative<bool>(needleVal) && std::holds_alternative<bool>(elem)) {
                                    if (std::get<bool>(elem) == std::get<bool>(needleVal)) { found = true; break; }
                                }
                            }
                        }
                    } else if (std::holds_alternative<std::shared_ptr<VMTome>>(collVal)) {
                        // For tome, check key membership
                        if (std::holds_alternative<std::string>(needleVal)) {
                            auto tome = std::get<std::shared_ptr<VMTome>>(collVal);
                            found = tome->entries.find(std::get<std::string>(needleVal)) != tome->entries.end();
                        }
                    }
                    push(found);
                    break;
                }
                case OpCode::OP_ITER_INIT: {
                    // pop collection, push iterator
                    if (stack.empty()) return {std::nullopt, false};
                    Value collVal = pop();
                    VMIterator iter;
                    if (std::holds_alternative<std::shared_ptr<VMOrder>>(collVal)) {
                        iter.kind = VMIterator::Kind::Order;
                        iter.orderRef = std::get<std::shared_ptr<VMOrder>>(collVal);
                    } else if (std::holds_alternative<std::shared_ptr<VMTome>>(collVal)) {
                        iter.kind = VMIterator::Kind::TomeKV;
                        iter.tomeRef = std::get<std::shared_ptr<VMTome>>(collVal);
                    } else return {std::nullopt, false};
                    iter.index = 0;
                    push(iter);
                    break;
                }
                case OpCode::OP_ITER_NEXT: {
                    // u16: jump offset if done
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t jumpOff = read_u16(&code[ip]); ip += 2;
                    if (stack.empty()) return {std::nullopt, false};
                    Value iterVal = pop();
                    if (!std::holds_alternative<VMIterator>(iterVal)) return {std::nullopt, false};
                    VMIterator iter = std::get<VMIterator>(iterVal);
                    if (iter.kind == VMIterator::Kind::Order) {
                        if (iter.index >= iter.orderRef->elements.size()) {
                            ip += jumpOff; // done
                        } else {
                            const auto& elem = iter.orderRef->elements[iter.index];
                            if (std::holds_alternative<int>(elem)) push(std::get<int>(elem));
                            else if (std::holds_alternative<std::string>(elem)) push(std::get<std::string>(elem));
                            else push(std::get<bool>(elem));
                            iter.index++;
                            push(iter);
                        }
                    } else {
                        // For single-value iteration on tome, just iterate keys
                        if (iter.index >= iter.tomeRef->keyOrder.size()) {
                            ip += jumpOff;
                        } else {
                            push(iter.tomeRef->keyOrder[iter.index]);
                            iter.index++;
                            push(iter);
                        }
                    }
                    break;
                }
                case OpCode::OP_ITER_KV_NEXT: {
                    // u16: jump offset if done - pushes key and value
                    if (ip + 1 >= code.size()) return {std::nullopt, false};
                    uint16_t jumpOff = read_u16(&code[ip]); ip += 2;
                    if (stack.empty()) return {std::nullopt, false};
                    Value iterVal = pop();
                    if (!std::holds_alternative<VMIterator>(iterVal)) return {std::nullopt, false};
                    VMIterator iter = std::get<VMIterator>(iterVal);
                    if (iter.kind != VMIterator::Kind::TomeKV) return {std::nullopt, false};
                    if (iter.index >= iter.tomeRef->keyOrder.size()) {
                        ip += jumpOff; // done
                    } else {
                        const std::string& key = iter.tomeRef->keyOrder[iter.index];
                        const auto& val = iter.tomeRef->entries.at(key);
                        push(key); // key first
                        if (std::holds_alternative<int>(val)) push(std::get<int>(val));
                        else if (std::holds_alternative<std::string>(val)) push(std::get<std::string>(val));
                        else push(std::get<bool>(val));
                        iter.index++;
                        push(iter);
                    }
                    break;
                }
                case OpCode::OP_CALL: {
                    // OP_CALL format: u16 funcId, u8 argc
                    if (ip + 2 >= code.size()) return {std::nullopt, false};
                    size_t callSiteOffset = ip - 1; // The opcode position
                    uint16_t funcId = read_u16(&code[ip]); ip += 2;
                    uint8_t argc = code[ip++];

                    // Check call site cache first
                    auto cacheIt = callSiteCache_.find(callSiteOffset);
                    if (cacheIt != callSiteCache_.end() && cacheIt->second.funcId == funcId) {
                        // Cache hit! Use cached entry point
                        CallSiteCache& cache = cacheIt->second;
                        cache.hitCount++;

                        // Verify arity matches
                        if (cache.arity != argc) {
                            return {std::nullopt, false}; // Arity mismatch
                        }

                        // For now, we don't have separate function chunks,
                        // but the cache infrastructure is ready for when we do.
                        // In a full implementation, we would:
                        // 1. Save return address
                        // 2. Jump to cache.entryPoint
                        // 3. Execute function code
                        // 4. Return to saved address
                    } else {
                        // Cache miss - resolve function
                        cacheMisses_++;

                        auto funcIt = functionTable_.find(funcId);
                        if (funcIt == functionTable_.end()) {
                            return {std::nullopt, false}; // Unknown function
                        }

                        const FunctionEntry& func = funcIt->second;
                        if (func.arity != argc) {
                            return {std::nullopt, false}; // Arity mismatch
                        }

                        // Populate cache for next time
                        CallSiteCache newCache;
                        newCache.funcId = funcId;
                        newCache.entryPoint = func.entryPoint;
                        newCache.arity = argc;
                        newCache.hitCount = 0;
                        callSiteCache_[callSiteOffset] = newCache;
                    }

                    // Placeholder: In a full implementation, we would execute the function here.
                    // For now, push a dummy return value.
                    push(0);
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
    std::unordered_map<size_t, CallSiteCache> callSiteCache_;  // Call site offset -> cache
    std::unordered_map<uint16_t, FunctionEntry> functionTable_; // Function ID -> entry
    size_t cacheMisses_{0};
};

} 

#endif 

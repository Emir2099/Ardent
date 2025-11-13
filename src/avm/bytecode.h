#ifndef ARDENT_AVM_BYTECODE_H
#define ARDENT_AVM_BYTECODE_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include "opcode.h"

namespace avm {

using Value = std::variant<int, std::string, bool>;

struct Chunk {
    std::vector<uint8_t> code;      // bytecode stream
    std::vector<Value> constants;   // constant pool
};

struct ModuleBytecode {
    Chunk main; // for now, single-chunk module
};

class BytecodeEmitter {
public:
    BytecodeEmitter() = default;
    uint16_t addConst(const Value& v) {
        constants_.push_back(v);
        return static_cast<uint16_t>(constants_.size() - 1);
    }
    void emit(OpCode op) { code_.push_back(static_cast<uint8_t>(op)); }
    void emit_u16(uint16_t v) { write_u16(v, code_); }

    // Convenience forms
    void emit_push_const(uint16_t idx) { emit(OpCode::OP_PUSH_CONST); emit_u16(idx); }
    void emit_halt() { emit(OpCode::OP_HALT); }

    Chunk build() {
        return Chunk{code_, constants_};
    }

private:
    std::vector<uint8_t> code_;
    std::vector<Value> constants_;
};

} 

#endif 

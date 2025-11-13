#ifndef ARDENT_AVM_OPCODE_H
#define ARDENT_AVM_OPCODE_H

#include <cstdint>
#include <vector>

namespace avm {

// Fixed-width opcodes. Operands follow opcode in little-endian order.
enum class OpCode : uint8_t {
    OP_NOP = 0x00,
    OP_PUSH_CONST = 0x01,   // u16: const index
    OP_POP = 0x02,

    // Variables (simple flat slots for now)
    OP_LOAD = 0x10,         // u16: slot id
    OP_STORE = 0x11,        // u16: slot id

    // Arithmetic / Logic
    OP_ADD = 0x20,
    OP_SUB = 0x21,
    OP_MUL = 0x22,
    OP_DIV = 0x23,
    OP_AND = 0x24,
    OP_OR  = 0x25,
    OP_NOT = 0x26,

    // Control Flow
    OP_JMP = 0x30,          // u16: rel offset
    OP_JMP_IF_FALSE = 0x31, // u16: rel offset

    // Calls & Returns (placeholders)
    OP_CALL = 0x40,         // u16: func id, u8 argc
    OP_RET  = 0x41,

    // Collections (placeholders)
    OP_MAKE_ORDER = 0x50,   // u16: count
    OP_MAKE_TOME  = 0x51,   // u16: count (key/value pairs)

    // Native bridge
    OP_NATIVE = 0x60,       // u16: native id, u8 argc

    // I/O & Debug
    OP_PRINT = 0x70,
    OP_DISCARD = 0x71,

    // Program control
    OP_HALT = 0xFF
};

inline uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline void write_u16(uint16_t v, std::vector<uint8_t>& out) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

}

#endif 

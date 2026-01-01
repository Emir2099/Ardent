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

    // Comparisons
    OP_EQ  = 0x27,
    OP_NE  = 0x28,
    OP_GT  = 0x29,
    OP_LT  = 0x2A,
    OP_GE  = 0x2B,
    OP_LE  = 0x2C,

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

    // Async / Concurrency (Ardent 2.4)
    OP_AWAIT = 0x80,        // Suspend until promise resolves
    OP_RESUME = 0x81,       // Resume suspended task with value
    OP_YIELD = 0x82,        // Yield control to scheduler
    OP_SPAWN = 0x83,        // Spawn a new task: u16 func_id
    OP_TASK_ID = 0x84,      // Push current task ID

    // Stream I/O (Ardent 2.4)
    OP_STREAM_OPEN = 0x90,  // u8: mode, open stream from path on stack
    OP_STREAM_CLOSE = 0x91, // Close stream ID on stack
    OP_STREAM_READ = 0x92,  // Read line from stream
    OP_STREAM_WRITE = 0x93, // Write to stream
    OP_STREAM_EOF = 0x94,   // Push true if stream at EOF

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

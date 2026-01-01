#ifndef ARDENT_AVM_DISASSEMBLER_H
#define ARDENT_AVM_DISASSEMBLER_H

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include "bytecode.h"
#include "opcode.h"

namespace avm {

inline std::string valueToString(const Value& v) {
    if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "True" : "False";
    const auto& s = std::get<std::string>(v);
    std::ostringstream oss; oss << '"' << s << '"';
    return oss.str();
}

inline std::string disassemble(const Chunk& chunk) {
    const auto& code = chunk.code;
    const auto& k = chunk.constants;
    std::ostringstream out;
    auto hex2 = [](uint16_t v){ std::ostringstream o; o<<std::hex<<v; return o.str(); };
    size_t ip = 0;
    while (ip < code.size()) {
        size_t offset = ip;
        OpCode op = static_cast<OpCode>(code[ip++]);
        out << (offset < 1000 ? (offset < 100 ? (offset < 10 ? "000" : "00") : "0") : "")
            << offset << ": ";
        switch (op) {
            case OpCode::OP_NOP: out << "NOP"; break;
            case OpCode::OP_HALT: out << "HALT"; break;
            case OpCode::OP_PUSH_CONST: {
                uint16_t idx = read_u16(&code[ip]); ip += 2;
                out << "PUSH_CONST " << idx;
                if (idx < k.size()) out << " ; " << valueToString(k[idx]);
                break;
            }
            case OpCode::OP_POP: out << "POP"; break;
            case OpCode::OP_LOAD: {
                uint16_t slot = read_u16(&code[ip]); ip += 2;
                out << "LOAD s" << slot; break;
            }
            case OpCode::OP_STORE: {
                uint16_t slot = read_u16(&code[ip]); ip += 2;
                out << "STORE s" << slot; break;
            }
            case OpCode::OP_ADD: out << "ADD"; break;
            case OpCode::OP_SUB: out << "SUB"; break;
            case OpCode::OP_MUL: out << "MUL"; break;
            case OpCode::OP_DIV: out << "DIV"; break;
            case OpCode::OP_AND: out << "AND"; break;
            case OpCode::OP_OR:  out << "OR"; break;
            case OpCode::OP_NOT: out << "NOT"; break;
            case OpCode::OP_EQ:  out << "EQ"; break;
            case OpCode::OP_NE:  out << "NE"; break;
            case OpCode::OP_GT:  out << "GT"; break;
            case OpCode::OP_LT:  out << "LT"; break;
            case OpCode::OP_GE:  out << "GE"; break;
            case OpCode::OP_LE:  out << "LE"; break;
            case OpCode::OP_JMP: {
                uint16_t off = read_u16(&code[ip]); ip += 2;
                out << "JMP +" << off << " -> " << (ip + off); break;
            }
            case OpCode::OP_JMP_IF_FALSE: {
                uint16_t off = read_u16(&code[ip]); ip += 2;
                out << "JMP_IF_FALSE +" << off << " -> " << (ip + off); break;
            }
            case OpCode::OP_PRINT: out << "PRINT"; break;
            case OpCode::OP_MAKE_ORDER: {
                uint16_t n = read_u16(&code[ip]); ip += 2;
                out << "MAKE_ORDER " << n; break;
            }
            case OpCode::OP_MAKE_TOME: {
                uint16_t n = read_u16(&code[ip]); ip += 2;
                out << "MAKE_TOME " << n; break;
            }
            case OpCode::OP_CALL: {
                uint16_t fid = read_u16(&code[ip]); ip += 2;
                uint8_t argc = code[ip++];
                out << "CALL f" << fid << " " << (int)argc; break;
            }
            case OpCode::OP_RET: out << "RET"; break;
            case OpCode::OP_NATIVE: {
                uint16_t nid = read_u16(&code[ip]); ip += 2;
                uint8_t argc = code[ip++];
                out << "NATIVE n" << nid << " " << (int)argc; break;
            }
            // Async/Concurrency (2.4)
            case OpCode::OP_AWAIT: out << "AWAIT"; break;
            case OpCode::OP_RESUME: out << "RESUME"; break;
            case OpCode::OP_YIELD: out << "YIELD"; break;
            case OpCode::OP_SPAWN: {
                uint16_t fid = read_u16(&code[ip]); ip += 2;
                out << "SPAWN f" << fid; break;
            }
            case OpCode::OP_TASK_ID: out << "TASK_ID"; break;
            // Stream I/O (2.4)
            case OpCode::OP_STREAM_OPEN: {
                uint8_t mode = code[ip++];
                out << "STREAM_OPEN mode=" << (int)mode; break;
            }
            case OpCode::OP_STREAM_CLOSE: out << "STREAM_CLOSE"; break;
            case OpCode::OP_STREAM_READ: out << "STREAM_READ"; break;
            case OpCode::OP_STREAM_WRITE: out << "STREAM_WRITE"; break;
            case OpCode::OP_STREAM_EOF: out << "STREAM_EOF"; break;
            default:
                out << "UNKNOWN(0x" << hex2(static_cast<uint16_t>(code[offset])) << ")"; break;
        }
        out << "\n";
    }
    return out.str();
}

} // namespace avm

#endif // ARDENT_AVM_DISASSEMBLER_H

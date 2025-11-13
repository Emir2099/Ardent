#ifndef ARDENT_AVM_BYTECODE_H
#define ARDENT_AVM_BYTECODE_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <fstream>
#include <cstring>
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
    size_t codeSize() const { return code_.size(); }
    uint8_t* rawData() { return code_.data(); }
    const std::vector<uint8_t>& codeRef() const { return code_; }

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

// Simple .avm binary format:
// magic: 'A''V''M''1'
// u16: constants count
//   for each const: u8 type (0=int,1=string,2=bool)
//                   if int:  i32 value
//                   if str:  u32 len, then bytes
//                   if bool: u8 0/1
// u32: code size
// code bytes

namespace avm_io {

inline bool save_chunk(const avm::Chunk& c, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const char magic[4] = {'A','V','M','1'};
    f.write(magic, 4);
    uint16_t kcount = static_cast<uint16_t>(c.constants.size());
    f.write(reinterpret_cast<const char*>(&kcount), sizeof(kcount));
    for (const auto& v : c.constants) {
        if (std::holds_alternative<int>(v)) {
            uint8_t t = 0; f.write(reinterpret_cast<const char*>(&t), 1);
            int32_t x = static_cast<int32_t>(std::get<int>(v));
            f.write(reinterpret_cast<const char*>(&x), sizeof(x));
        } else if (std::holds_alternative<std::string>(v)) {
            uint8_t t = 1; f.write(reinterpret_cast<const char*>(&t), 1);
            const auto& s = std::get<std::string>(v);
            uint32_t len = static_cast<uint32_t>(s.size());
            f.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len) f.write(s.data(), len);
        } else { // bool
            uint8_t t = 2; f.write(reinterpret_cast<const char*>(&t), 1);
            uint8_t b = std::get<bool>(v) ? 1 : 0;
            f.write(reinterpret_cast<const char*>(&b), 1);
        }
    }
    uint32_t codesz = static_cast<uint32_t>(c.code.size());
    f.write(reinterpret_cast<const char*>(&codesz), sizeof(codesz));
    if (codesz) f.write(reinterpret_cast<const char*>(c.code.data()), codesz);
    return static_cast<bool>(f);
}

inline bool load_chunk(const std::string& path, avm::Chunk& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4];
    if (!f.read(magic, 4)) return false;
    if (!(magic[0]=='A'&&magic[1]=='V'&&magic[2]=='M'&&magic[3]=='1')) return false;
    uint16_t kcount = 0;
    if (!f.read(reinterpret_cast<char*>(&kcount), sizeof(kcount))) return false;
    out.constants.clear();
    out.constants.reserve(kcount);
    for (uint16_t i=0;i<kcount;++i) {
        uint8_t t=0; if (!f.read(reinterpret_cast<char*>(&t),1)) return false;
        if (t==0) { int32_t x; if (!f.read(reinterpret_cast<char*>(&x),4)) return false; out.constants.emplace_back(static_cast<int>(x)); }
        else if (t==1) { uint32_t len=0; if (!f.read(reinterpret_cast<char*>(&len),4)) return false; std::string s; s.resize(len); if (len && !f.read(&s[0], len)) return false; out.constants.emplace_back(std::move(s)); }
        else if (t==2) { uint8_t b=0; if (!f.read(reinterpret_cast<char*>(&b),1)) return false; out.constants.emplace_back(b!=0); }
        else return false;
    }
    uint32_t codesz=0; if (!f.read(reinterpret_cast<char*>(&codesz),4)) return false;
    out.code.resize(codesz);
    if (codesz && !f.read(reinterpret_cast<char*>(out.code.data()), codesz)) return false;
    return true;
}

inline bool is_avm_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4];
    if (!f.read(magic, 4)) return false;
    return magic[0]=='A' && magic[1]=='V' && magic[2]=='M' && magic[3]=='1';
}

} // namespace avm_io

#endif 

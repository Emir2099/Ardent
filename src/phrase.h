#ifndef PHRASE_H
#define PHRASE_H
#include <cstdint>
#include <cstring>
#include "arena.h"

struct Phrase {
    // SSO threshold (<=23 chars)
    static constexpr std::uint8_t SSO_MAX = 23;
    std::uint8_t len; // length
    bool small;       // true if data fits inline
    union {
        char inlineData[SSO_MAX + 1]; // +1 for optional terminator (not required logically)
        struct { const char* ptr; std::uint32_t heapLen; } ref; // arena-backed
    } u;

    Phrase() : len(0), small(true) { u.inlineData[0] = '\0'; }

    static Phrase make(const char* s, std::size_t n, Arena& arena) {
        Phrase p;
        if (n <= SSO_MAX) {
            p.small = true; p.len = (std::uint8_t)n;
            std::memcpy(p.u.inlineData, s, n);
            p.u.inlineData[n] = '\0';
        } else {
            p.small = false; p.len = (std::uint8_t)(n & 0xFF); // store mod 256; full length in heapLen
            char* buf = static_cast<char*>(arena.alloc(n));
            std::memcpy(buf, s, n);
            p.u.ref.ptr = buf; p.u.ref.heapLen = (std::uint32_t)n;
        }
        return p;
    }

    const char* data() const { return small ? u.inlineData : u.ref.ptr; }
    std::size_t size() const { return small ? len : u.ref.heapLen; }
};

inline Phrase concat(const Phrase& a, const Phrase& b, Arena& arena) {
    std::size_t total = a.size() + b.size();
    if (total <= Phrase::SSO_MAX) {
        Phrase p; p.small = true; p.len = (std::uint8_t)total;
        std::memcpy(p.u.inlineData, a.data(), a.size());
        std::memcpy(p.u.inlineData + a.size(), b.data(), b.size());
        p.u.inlineData[total] = '\0';
        return p;
    }
    char* buf = static_cast<char*>(arena.alloc(total));
    std::memcpy(buf, a.data(), a.size());
    std::memcpy(buf + a.size(), b.data(), b.size());
    Phrase p; p.small = false; p.len = (std::uint8_t)(total & 0xFF);
    p.u.ref.ptr = buf; p.u.ref.heapLen = (std::uint32_t)total;
    return p;
}

#endif // PHRASE_H

#ifndef ENV_H
#define ENV_H
#include <cstdint>
#include <vector>
#include <cstring>
#include <utility>
#include "arena.h"
#include "phrase.h"

// A compact key stored in arena (no std::string)
struct KeyRef {
    const char* data {nullptr};
    std::uint32_t len {0};
};

inline bool keyEq(const KeyRef& a, const KeyRef& b) {
    if (a.len != b.len) return false;
    if (a.data == b.data) return true;
    return std::memcmp(a.data, b.data, a.len) == 0;
}

inline std::uint64_t fnv1a64(const char* d, std::size_t n) {
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < n; ++i) { h ^= (std::uint8_t)d[i]; h *= 1099511628211ull; }
    return h;
}

template <typename V>
struct EnvEntry {
    KeyRef key;
    V value;
    std::uint64_t hash {0};
    bool occupied {false};
};

template <typename V>
class ScopedMap {
public:
    explicit ScopedMap(Arena& arena, std::size_t initial = 32)
        : arena_(arena) {
        capacity_ = nextPow2(initial);
        entries_ = static_cast<Entry*>(arena_.alloc(sizeof(Entry) * capacity_, alignof(Entry)));
        std::memset(entries_, 0, sizeof(Entry) * capacity_);
    }

    bool put(const char* keyBytes, std::uint32_t keyLen, std::uint64_t h, const V& v) {
        KeyRef k;
        // Copy key into arena once on first insert
        char* buf = static_cast<char*>(arena_.alloc(keyLen));
        std::memcpy(buf, keyBytes, keyLen);
        k.data = buf; k.len = keyLen;
        std::size_t mask = capacity_ - 1;
        for (std::size_t i = 0; i < capacity_; ++i) {
            std::size_t idx = (h + i) & mask;
            Entry& e = entries_[idx];
            if (!e.occupied || (e.hash == h && keyEq(e.key, k))) {
                if (!e.occupied) { e.key = k; e.hash = h; e.occupied = true; }
                e.value = v; return true;
            }
        }
        return false; // full; for brevity no rehash right now
    }

    V* get(const char* keyBytes, std::uint32_t keyLen, std::uint64_t h) {
        KeyRef k{ keyBytes, keyLen };
        std::size_t mask = capacity_ - 1;
        for (std::size_t i = 0; i < capacity_; ++i) {
            std::size_t idx = (h + i) & mask;
            Entry& e = entries_[idx];
            if (!e.occupied) return nullptr;
            if (e.hash == h && keyEq(e.key, k)) return &e.value;
        }
        return nullptr;
    }

private:
    Arena& arena_;
    using Entry = EnvEntry<V>;
    Entry* entries_ {nullptr};
    std::size_t capacity_ {0};

    static std::size_t nextPow2(std::size_t n) {
        std::size_t p = 1; while (p < n) p <<= 1; return p;
    }
};

template <typename V>
class EnvStack {
public:
    void push(Arena& a) { maps_.emplace_back(a); }
    void pop() { maps_.pop_back(); }
    bool declare(const char* key, std::uint32_t len, const V& v) {
        auto h = fnv1a64(key, len);
        return maps_.back().put(key, len, h, v);
    }
    V* lookup(const char* key, std::uint32_t len) {
        auto h = fnv1a64(key, len);
        for (std::size_t i = maps_.size(); i-- > 0;) if (auto* v = maps_[i].get(key, len, h)) return v;
        return nullptr;
    }
    // Assign to an existing variable in the nearest (innermost) scope where it appears.
    // If not found, implicitly declares in the current (top) scope.
    void assign(const char* key, std::uint32_t len, const V& v) {
        auto h = fnv1a64(key, len);
        for (std::size_t i = maps_.size(); i-- > 0;) {
            if (auto* existing = maps_[i].get(key, len, h)) { *existing = v; return; }
        }
        // Not found: declare in current map
        maps_.back().put(key, len, h, v);
    }
private:
    std::vector<ScopedMap<V>> maps_;
};

#endif // ENV_H

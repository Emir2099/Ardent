#ifndef ARENA_H
#define ARENA_H
#include <cstddef>
#include <cstdint>
#include <vector>
#include <new>
#include <utility>
#include <algorithm>

class Arena {
public:
    struct Block {
        std::uint8_t* data;
        std::size_t   capacity;
        std::size_t   offset;
    };
    struct Frame {
        std::size_t blockIndex;
        std::size_t offset;
    };

    explicit Arena(std::size_t initial = 1 << 16) { addBlock(initial); }
    ~Arena() { for (auto &b : blocks_) delete[] b.data; }

    Arena(const Arena&) = delete; Arena& operator=(const Arena&) = delete;

    void* alloc(std::size_t n, std::size_t align = alignof(std::max_align_t)) {
        if (n == 0) return nullptr;
        // Align current offset
        Block* b = &blocks_.back();
        std::size_t current = b->offset;
        std::size_t aligned = (current + (align - 1)) & ~(align - 1);
        if (aligned + n > b->capacity) {
            // grow: next block at least double or large enough
            std::size_t newCap = std::max<std::size_t>(n + align, b->capacity * 2);
            addBlock(newCap);
            b = &blocks_.back();
            aligned = 0;
        }
        void* ptr = b->data + aligned;
        b->offset = aligned + n;
        return ptr;
    }

    template<class T, class... A>
    T* make(A&&... a) {
        void* mem = alloc(sizeof(T), alignof(T));
        return new (mem) T(std::forward<A>(a)...);
    }

    Frame pushFrame() const {
        return Frame{ blocks_.size() - 1, blocks_.back().offset };
    }

    void popFrame(const Frame& f) {
        if (f.blockIndex >= blocks_.size()) return; // invalid
        // discard newer blocks
        while (blocks_.size() - 1 > f.blockIndex) {
            delete[] blocks_.back().data;
            blocks_.pop_back();
        }
        blocks_.back().offset = f.offset;
    }

    // Report total bytes currently occupied across all blocks (high-water offsets).
    // Useful for coarse memory usage/benchmarking.
    std::size_t bytesUsed() const {
        std::size_t sum = 0;
        for (const auto &b : blocks_) sum += b.offset;
        return sum;
    }

private:
    std::vector<Block> blocks_;
    void addBlock(std::size_t cap) {
        Block b{ new std::uint8_t[cap], cap, 0 };
        blocks_.push_back(b);
    }
};

#endif // ARENA_H

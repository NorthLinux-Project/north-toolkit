#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <utility>

namespace north::core {

class Arena {
public:
    explicit Arena(size_t initial_chunk_size = 1024 * 1024) noexcept;
    ~Arena() noexcept;

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&& other) noexcept;
    Arena& operator=(Arena&& other) noexcept;

    [[nodiscard]] void* alloc(size_t size, size_t alignment = alignof(std::max_align_t)) noexcept;

    template <typename T, typename... Args>
    [[nodiscard]] T* create(Args&&... args) noexcept {
        void* mem = alloc(sizeof(T), alignof(T));
        if (!mem) {
            return nullptr;
        }
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    template <typename T>
    [[nodiscard]] std::span<T> alloc_array(size_t count) noexcept {
        if (count == 0) {
            return {};
        }
        if (count > SIZE_MAX / sizeof(T)) {
            return {};
        }
        void* mem = alloc(sizeof(T) * count, alignof(T));
        if (!mem) {
            return {};
        }
        return std::span<T>(static_cast<T*>(mem), count);
    }

    void reset() noexcept;

    [[nodiscard]] size_t used() const noexcept;
    [[nodiscard]] size_t capacity() const noexcept;

private:
    struct Chunk {
        Chunk* next{nullptr};
        size_t capacity{0};
        size_t offset{0};
        alignas(std::max_align_t) uint8_t data[1];
    };

    Chunk* head_{nullptr};
    Chunk* current_{nullptr};
    size_t default_chunk_size_{0};
    size_t total_capacity_{0};

    [[nodiscard]] Chunk* allocate_chunk(size_t min_capacity) noexcept;
};

} // namespace north::core

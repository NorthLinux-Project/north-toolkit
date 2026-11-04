#include "north/core/arena.hpp"

#include <algorithm>
#include <cstdlib>

namespace north::core {

namespace {

[[nodiscard]] constexpr bool is_power_of_two(size_t x) noexcept {
    return x != 0 && (x & (x - 1)) == 0;
}

} // namespace

Arena::Arena(size_t initial_chunk_size) noexcept
    : default_chunk_size_(std::max<size_t>(initial_chunk_size, 4096)) {
    head_ = allocate_chunk(default_chunk_size_);
    current_ = head_;
}

Arena::~Arena() noexcept {
    Chunk* chunk = head_;
    while (chunk) {
        Chunk* next = chunk->next;
        std::free(chunk);
        chunk = next;
    }
    head_ = nullptr;
    current_ = nullptr;
    total_capacity_ = 0;
}

Arena::Arena(Arena&& other) noexcept
    : head_(other.head_), current_(other.current_), default_chunk_size_(other.default_chunk_size_),
      total_capacity_(other.total_capacity_) {
    other.head_ = nullptr;
    other.current_ = nullptr;
    other.total_capacity_ = 0;
}

Arena& Arena::operator=(Arena&& other) noexcept {
    if (this != &other) {
        Chunk* chunk = head_;
        while (chunk) {
            Chunk* next = chunk->next;
            std::free(chunk);
            chunk = next;
        }

        head_ = other.head_;
        current_ = other.current_;
        default_chunk_size_ = other.default_chunk_size_;
        total_capacity_ = other.total_capacity_;

        other.head_ = nullptr;
        other.current_ = nullptr;
        other.total_capacity_ = 0;
    }
    return *this;
}

Arena::Chunk* Arena::allocate_chunk(size_t min_capacity) noexcept {
    size_t chunk_cap = std::max(default_chunk_size_, min_capacity);
    size_t total_alloc = sizeof(Chunk) - 1 + chunk_cap;
    void* mem = std::malloc(total_alloc);
    if (!mem) {
        return nullptr;
    }

    auto* chunk = static_cast<Chunk*>(mem);
    chunk->next = nullptr;
    chunk->capacity = chunk_cap;
    chunk->offset = 0;
    total_capacity_ += chunk_cap;
    return chunk;
}

void* Arena::alloc(size_t size, size_t alignment) noexcept {
    if (size == 0 || !is_power_of_two(alignment)) {
        return nullptr;
    }

    if (!current_) {
        head_ = allocate_chunk(std::max(default_chunk_size_, size));
        current_ = head_;
        if (!current_) {
            return nullptr;
        }
    }

    uintptr_t data_base = reinterpret_cast<uintptr_t>(current_->data);
    uintptr_t cur_ptr = data_base + current_->offset;
    uintptr_t aligned_ptr = (cur_ptr + alignment - 1) & ~(alignment - 1);
    size_t needed_offset = static_cast<size_t>(aligned_ptr - data_base) + size;

    if (needed_offset <= current_->capacity) {
        current_->offset = needed_offset;
        return reinterpret_cast<void*>(aligned_ptr);
    }

    if (current_->next) {
        Chunk* prev = current_;
        current_ = current_->next;
        current_->offset = 0;

        data_base = reinterpret_cast<uintptr_t>(current_->data);
        cur_ptr = data_base;
        aligned_ptr = (cur_ptr + alignment - 1) & ~(alignment - 1);
        needed_offset = static_cast<size_t>(aligned_ptr - data_base) + size;

        if (needed_offset <= current_->capacity) {
            current_->offset = needed_offset;
            return reinterpret_cast<void*>(aligned_ptr);
        }
        current_ = prev;
    }

    size_t new_chunk_cap = std::max(default_chunk_size_, size + alignment);
    Chunk* new_chunk = allocate_chunk(new_chunk_cap);
    if (!new_chunk) {
        return nullptr;
    }

    new_chunk->next = current_->next;
    current_->next = new_chunk;
    current_ = new_chunk;

    data_base = reinterpret_cast<uintptr_t>(current_->data);
    cur_ptr = data_base;
    aligned_ptr = (cur_ptr + alignment - 1) & ~(alignment - 1);
    current_->offset = static_cast<size_t>(aligned_ptr - data_base) + size;
    return reinterpret_cast<void*>(aligned_ptr);
}

void Arena::reset() noexcept {
    Chunk* chunk = head_;
    while (chunk) {
        chunk->offset = 0;
        chunk = chunk->next;
    }
    current_ = head_;
}

size_t Arena::used() const noexcept {
    size_t total = 0;
    const Chunk* chunk = head_;
    while (chunk) {
        total += chunk->offset;
        if (chunk == current_) {
            break;
        }
        chunk = chunk->next;
    }
    return total;
}

size_t Arena::capacity() const noexcept {
    return total_capacity_;
}

} // namespace north::core

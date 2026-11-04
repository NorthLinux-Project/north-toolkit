#pragma once

#include "north/core/assert.hpp"
#include "north/core/handle.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <vector>

namespace north::core {

template <typename T, size_t ChunkCapacity = 64>
class SlabPool {
public:
    using HandleType = Handle<T>;

    SlabPool() = default;

    ~SlabPool() noexcept {
        clear();
        for (Chunk* chunk : chunks_) {
            delete chunk;
        }
        chunks_.clear();
    }

    SlabPool(const SlabPool&) = delete;
    SlabPool& operator=(const SlabPool&) = delete;

    SlabPool(SlabPool&& other) noexcept
        : chunks_(std::move(other.chunks_)), free_list_(std::move(other.free_list_)),
          total_slots_(other.total_slots_), occupied_count_(other.occupied_count_) {
        other.total_slots_ = 0;
        other.occupied_count_ = 0;
    }

    SlabPool& operator=(SlabPool&& other) noexcept {
        if (this != &other) {
            clear();
            for (Chunk* chunk : chunks_) {
                delete chunk;
            }
            chunks_ = std::move(other.chunks_);
            free_list_ = std::move(other.free_list_);
            total_slots_ = other.total_slots_;
            occupied_count_ = other.occupied_count_;

            other.total_slots_ = 0;
            other.occupied_count_ = 0;
        }
        return *this;
    }

    template <typename... Args>
    [[nodiscard]] HandleType emplace(Args&&... args) {
        if (free_list_.empty()) {
            grow_capacity();
        }

        uint32_t slot_idx = free_list_.back();
        free_list_.pop_back();

        Slot& slot = get_slot(slot_idx);
        NORTH_ASSERT(!slot.occupied, "SlabPool target slot must not be occupied");

        ::new (static_cast<void*>(slot.storage)) T(std::forward<Args>(args)...);
        slot.occupied = true;
        if (slot.generation == 0) {
            slot.generation = 1;
        }
        ++occupied_count_;

        return HandleType{.index = slot_idx, .generation = slot.generation};
    }

    void destroy(HandleType h) noexcept {
        if (!is_valid(h)) {
            return;
        }

        Slot& slot = get_slot(h.index);
        reinterpret_cast<T*>(slot.storage)->~T();
        slot.occupied = false;
        ++slot.generation;
        if (slot.generation == 0) {
            slot.generation = 1;
        }

        free_list_.push_back(h.index);
        --occupied_count_;
    }

    [[nodiscard]] T* get(HandleType h) noexcept {
        if (!is_valid(h)) {
            return nullptr;
        }
        return reinterpret_cast<T*>(get_slot(h.index).storage);
    }

    [[nodiscard]] const T* get(HandleType h) const noexcept {
        if (!is_valid(h)) {
            return nullptr;
        }
        return reinterpret_cast<const T*>(get_slot(h.index).storage);
    }

    [[nodiscard]] bool is_valid(HandleType h) const noexcept {
        if (!h.is_valid() || h.index >= total_slots_) {
            return false;
        }
        const Slot& slot = get_slot(h.index);
        return slot.occupied && slot.generation == h.generation;
    }

    [[nodiscard]] size_t size() const noexcept { return occupied_count_; }

    [[nodiscard]] size_t capacity() const noexcept { return total_slots_; }

    template <typename Fn>
    void for_each(Fn&& fn) {
        for (uint32_t i = 0; i < total_slots_; ++i) {
            Slot& slot = get_slot(i);
            if (slot.occupied) {
                fn(HandleType{.index = i, .generation = slot.generation},
                   *reinterpret_cast<T*>(slot.storage));
            }
        }
    }

    template <typename Fn>
    void for_each(Fn&& fn) const {
        for (uint32_t i = 0; i < total_slots_; ++i) {
            const Slot& slot = get_slot(i);
            if (slot.occupied) {
                fn(HandleType{.index = i, .generation = slot.generation},
                   *reinterpret_cast<const T*>(slot.storage));
            }
        }
    }

    void clear() noexcept {
        for (uint32_t i = 0; i < total_slots_; ++i) {
            Slot& slot = get_slot(i);
            if (slot.occupied) {
                reinterpret_cast<T*>(slot.storage)->~T();
                slot.occupied = false;
                ++slot.generation;
                if (slot.generation == 0) {
                    slot.generation = 1;
                }
            }
        }
        occupied_count_ = 0;
        free_list_.clear();
        free_list_.reserve(total_slots_);
        for (size_t i = total_slots_; i > 0; --i) {
            free_list_.push_back(static_cast<uint32_t>(i - 1));
        }
    }

private:
    struct Slot {
        alignas(T) uint8_t storage[sizeof(T)];
        uint32_t generation{1};
        bool occupied{false};
    };

    struct Chunk {
        Slot slots[ChunkCapacity];
    };

    std::vector<Chunk*> chunks_;
    std::vector<uint32_t> free_list_;
    size_t total_slots_{0};
    size_t occupied_count_{0};

    void grow_capacity() {
        auto* chunk = new Chunk();
        chunks_.push_back(chunk);
        uint32_t base_idx = static_cast<uint32_t>(total_slots_);
        total_slots_ += ChunkCapacity;

        for (size_t i = ChunkCapacity; i > 0; --i) {
            free_list_.push_back(base_idx + static_cast<uint32_t>(i - 1));
        }
    }

    [[nodiscard]] Slot& get_slot(uint32_t index) noexcept {
        return chunks_[index / ChunkCapacity]->slots[index % ChunkCapacity];
    }

    [[nodiscard]] const Slot& get_slot(uint32_t index) const noexcept {
        return chunks_[index / ChunkCapacity]->slots[index % ChunkCapacity];
    }
};

} // namespace north::core

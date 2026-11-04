#include "north/core/arena.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <span>

namespace north::core::testing {

class ArenaTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        arena = std::make_unique<Arena>(64 * 1024); // 64 KB initial chunk
    }

    void TearDown() override { arena.reset(); }

    std::unique_ptr<Arena> arena;
};

TEST_F(ArenaTestFixture, AllocatesAlignedMemory) {
    void* p1 = arena->alloc(13, 8);
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p1) % 8, 0u);

    void* p2 = arena->alloc(7, 16);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p2) % 16, 0u);

    EXPECT_GT(reinterpret_cast<uintptr_t>(p2), reinterpret_cast<uintptr_t>(p1));
}

TEST_F(ArenaTestFixture, ConsecutiveAllocationsAdvanceUsedOffset) {
    size_t before = arena->used();
    void* p = arena->alloc(128, 8);
    ASSERT_NE(p, nullptr);
    size_t after = arena->used();

    EXPECT_GE(after - before, 128u);
}

TEST_F(ArenaTestFixture, InPlaceObjectConstruction) {
    struct ComplexObject {
        int x;
        float y;
        double z;
        bool constructed{true};

        ComplexObject(int x_val, float y_val, double z_val)
            : x(x_val), y(y_val), z(z_val), constructed(true) {}
    };

    auto* obj = arena->create<ComplexObject>(10, 20.5f, 30.125);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->x, 10);
    EXPECT_FLOAT_EQ(obj->y, 20.5f);
    EXPECT_DOUBLE_EQ(obj->z, 30.125);
    EXPECT_TRUE(obj->constructed);
}

TEST_F(ArenaTestFixture, ContiguousArrayAllocation) {
    constexpr size_t count = 100;
    std::span<int32_t> array = arena->alloc_array<int32_t>(count);
    ASSERT_EQ(array.size(), count);
    ASSERT_NE(array.data(), nullptr);

    for (size_t i = 0; i < count; ++i) {
        array[i] = static_cast<int32_t>(i * 3);
    }

    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(array[i], static_cast<int32_t>(i * 3));
    }
}

TEST_F(ArenaTestFixture, ResetReclaimsMemoryAndRetainsCapacity) {
    size_t cap_initial = arena->capacity();

    void* p1 = arena->alloc(1024, 8);
    void* p2 = arena->alloc(2048, 8);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_GT(arena->used(), 0u);

    arena->reset();
    EXPECT_EQ(arena->used(), 0u);
    EXPECT_EQ(arena->capacity(), cap_initial);

    // Can allocate again after reset
    void* p = arena->alloc(512, 8);
    ASSERT_NE(p, nullptr);
    EXPECT_GE(arena->used(), 512u);
}

TEST_F(ArenaTestFixture, ChunkedGrowthExceedingInitialCapacity) {
    // Initial chunk is 64 KB; allocate 128 KB total across chunks
    constexpr size_t large_alloc = 80 * 1024;
    void* p1 = arena->alloc(large_alloc, 16);
    ASSERT_NE(p1, nullptr);

    void* p2 = arena->alloc(large_alloc, 16);
    ASSERT_NE(p2, nullptr);

    EXPECT_GT(arena->capacity(), 64 * 1024u);
    EXPECT_GE(arena->used(), large_alloc * 2);
}

TEST_F(ArenaTestFixture, ZeroByteAllocationReturnsNullptr) {
    void* p = arena->alloc(0, 8);
    EXPECT_EQ(p, nullptr);

    auto empty_span = arena->alloc_array<float>(0);
    EXPECT_TRUE(empty_span.empty());
}

TEST_F(ArenaTestFixture, InvalidNonPowerOfTwoAlignmentReturnsNullptr) {
    void* p = arena->alloc(64, 7);
    EXPECT_EQ(p, nullptr);

    void* p_zero_align = arena->alloc(64, 0);
    EXPECT_EQ(p_zero_align, nullptr);
}

TEST_F(ArenaTestFixture, MoveConstructionTransfersBuffer) {
    void* p = arena->alloc(256, 8);
    ASSERT_NE(p, nullptr);
    size_t used_orig = arena->used();
    size_t cap_orig = arena->capacity();

    Arena moved_arena(std::move(*arena));
    EXPECT_EQ(moved_arena.used(), used_orig);
    EXPECT_EQ(moved_arena.capacity(), cap_orig);
    EXPECT_EQ(arena->capacity(), 0u);
}

} // namespace north::core::testing

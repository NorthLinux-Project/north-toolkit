#include "north/core/slab_pool.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace north::core::testing {

struct DestructorTracker {
    int id{0};
    int* dtor_counter{nullptr};

    DestructorTracker(int id_val, int* counter) : id(id_val), dtor_counter(counter) {}

    ~DestructorTracker() {
        if (dtor_counter) {
            ++(*dtor_counter);
        }
    }
};

class SlabPoolTestFixture : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SlabPoolTestFixture, EmplaceAndGet) {
    SlabPool<std::string, 8> pool;

    auto h1 = pool.emplace("Node Alpha");
    auto h2 = pool.emplace("Node Beta");

    ASSERT_TRUE(h1.is_valid());
    ASSERT_TRUE(h2.is_valid());
    EXPECT_NE(h1, h2);

    std::string* s1 = pool.get(h1);
    std::string* s2 = pool.get(h2);

    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);
    EXPECT_EQ(*s1, "Node Alpha");
    EXPECT_EQ(*s2, "Node Beta");
    EXPECT_EQ(pool.size(), 2u);
}

TEST_F(SlabPoolTestFixture, PointerStabilityAcrossChunkGrowth) {
    // Small chunk capacity (4 items per chunk) to trigger frequent chunk allocations
    SlabPool<int, 4> pool;

    std::vector<SlabPool<int, 4>::HandleType> handles;
    std::vector<int*> pointers;

    constexpr int count = 50;
    for (int i = 0; i < count; ++i) {
        auto h = pool.emplace(i * 10);
        handles.push_back(h);
        pointers.push_back(pool.get(h));
    }

    EXPECT_EQ(pool.size(), static_cast<size_t>(count));
    EXPECT_GE(pool.capacity(), static_cast<size_t>(count));

    // Verify pointer stability: every pointer recorded at emplace time must be identical
    for (int i = 0; i < count; ++i) {
        int* current_ptr = pool.get(handles[static_cast<size_t>(i)]);
        EXPECT_EQ(current_ptr, pointers[static_cast<size_t>(i)]);
        ASSERT_NE(current_ptr, nullptr);
        EXPECT_EQ(*current_ptr, i * 10);
    }
}

TEST_F(SlabPoolTestFixture, DestroyInvokesDestructorAndInvalidatesHandle) {
    SlabPool<DestructorTracker, 4> pool;
    int dtor_calls = 0;

    auto h = pool.emplace(42, &dtor_calls);
    EXPECT_TRUE(pool.is_valid(h));
    EXPECT_EQ(dtor_calls, 0);

    pool.destroy(h);
    EXPECT_EQ(dtor_calls, 1);
    EXPECT_FALSE(pool.is_valid(h));
    EXPECT_EQ(pool.get(h), nullptr);
    EXPECT_EQ(pool.size(), 0u);
}

TEST_F(SlabPoolTestFixture, ReusesSlotWithIncrementedGeneration) {
    SlabPool<int, 4> pool;

    auto h1 = pool.emplace(100);
    uint32_t original_index = h1.index;
    uint32_t original_generation = h1.generation;

    pool.destroy(h1);

    auto h2 = pool.emplace(200);
    EXPECT_EQ(h2.index, original_index);
    EXPECT_GT(h2.generation, original_generation);

    EXPECT_FALSE(pool.is_valid(h1));
    EXPECT_TRUE(pool.is_valid(h2));
    EXPECT_EQ(pool.get(h1), nullptr);
    ASSERT_NE(pool.get(h2), nullptr);
    EXPECT_EQ(*pool.get(h2), 200);
}

TEST_F(SlabPoolTestFixture, ForEachVisitsOnlyOccupiedSlots) {
    SlabPool<int, 4> pool;

    auto h1 = pool.emplace(1);
    auto h2 = pool.emplace(2);
    auto h3 = pool.emplace(3);

    pool.destroy(h2);

    int sum = 0;
    size_t visited = 0;
    pool.for_each([&](auto handle, int& val) {
        EXPECT_TRUE(handle == h1 || handle == h3);
        sum += val;
        ++visited;
    });

    EXPECT_EQ(visited, 2u);
    EXPECT_EQ(sum, 4);
}

TEST_F(SlabPoolTestFixture, ClearDestroysAllAndResets) {
    SlabPool<DestructorTracker, 4> pool;
    int dtor_calls = 0;

    auto h1 = pool.emplace(1, &dtor_calls);
    auto h2 = pool.emplace(2, &dtor_calls);
    EXPECT_EQ(pool.size(), 2u);

    pool.clear();
    EXPECT_EQ(dtor_calls, 2);
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_FALSE(pool.is_valid(h1));
    EXPECT_FALSE(pool.is_valid(h2));

    auto h3 = pool.emplace(3, &dtor_calls);
    EXPECT_TRUE(pool.is_valid(h3));
    EXPECT_EQ(pool.size(), 1u);
}

} // namespace north::core::testing

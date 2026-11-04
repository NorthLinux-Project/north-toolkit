#include "north/core/signal.hpp"

#include <gtest/gtest.h>

#include <string>

namespace north::core::testing {

struct CallbackRecord {
    int call_count{0};
    uint32_t last_flags{0};

    static void on_dirty(void* user_data, uint32_t flags) noexcept {
        auto* rec = static_cast<CallbackRecord*>(user_data);
        ++rec->call_count;
        rec->last_flags = flags;
    }
};

TEST(SignalTest, StoresInitialValueAndMutates) {
    Signal<int> sig(42);
    EXPECT_EQ(sig.get(), 42);

    sig.set(100);
    EXPECT_EQ(sig.get(), 100);
}

TEST(SignalTest, NotifiesObserversOnMutation) {
    Signal<std::string> sig("Initial");
    CallbackRecord rec;

    sig.bind_dirty(&CallbackRecord::on_dirty, &rec, kLayout | kPaint);
    EXPECT_EQ(rec.call_count, 0);

    sig.set("Updated");
    EXPECT_EQ(rec.call_count, 1);
    EXPECT_EQ(rec.last_flags, (kLayout | kPaint));

    // Setting same value must not trigger observers
    sig.set("Updated");
    EXPECT_EQ(rec.call_count, 1);
}

TEST(SignalTest, SmallBufferOptimizationAndOverflow) {
    Signal<int> sig(0);
    CallbackRecord r1, r2, r3, r4;

    sig.bind_dirty(&CallbackRecord::on_dirty, &r1, kLayout);
    sig.bind_dirty(&CallbackRecord::on_dirty, &r2, kPaint);
    EXPECT_EQ(sig.observer_count(), 2u);

    // 3rd and 4th observers exercise vector overflow
    sig.bind_dirty(&CallbackRecord::on_dirty, &r3, kTransform);
    sig.bind_dirty(&CallbackRecord::on_dirty, &r4, kChildren);
    EXPECT_EQ(sig.observer_count(), 4u);

    sig.set(5);
    EXPECT_EQ(r1.call_count, 1);
    EXPECT_EQ(r1.last_flags, kLayout);
    EXPECT_EQ(r2.call_count, 1);
    EXPECT_EQ(r2.last_flags, kPaint);
    EXPECT_EQ(r3.call_count, 1);
    EXPECT_EQ(r3.last_flags, kTransform);
    EXPECT_EQ(r4.call_count, 1);
    EXPECT_EQ(r4.last_flags, kChildren);
}

TEST(SignalTest, ComputedEvaluatesLazilyAndCaches) {
    Signal<int> a(10);
    Signal<int> b(20);
    int compute_invocations = 0;

    auto compute_sum = [&]() -> int {
        ++compute_invocations;
        return a.get() + b.get();
    };

    Computed<int, decltype(compute_sum)> sum(compute_sum);

    EXPECT_EQ(compute_invocations, 0);

    // First read invokes compute function
    EXPECT_EQ(sum.get(), 30);
    EXPECT_EQ(compute_invocations, 1);

    // Second read uses cached result without re-invoking
    EXPECT_EQ(sum.get(), 30);
    EXPECT_EQ(compute_invocations, 1);

    // Bind computed to signals
    a.bind_dirty(&decltype(sum)::on_dependency_dirty, &sum);
    b.bind_dirty(&decltype(sum)::on_dependency_dirty, &sum);

    // Mutating a dependency invalidates cache
    a.set(15);
    EXPECT_TRUE(sum.is_dirty());
    EXPECT_EQ(compute_invocations, 1); // Not evaluated yet!

    // Next read re-evaluates
    EXPECT_EQ(sum.get(), 35);
    EXPECT_EQ(compute_invocations, 2);
}

TEST(SignalTest, EffectExecutesOnDependencyChange) {
    Signal<int> sig(1);
    int effect_runs = 0;

    auto action = [&]() { ++effect_runs; };

    Effect<decltype(action)> eff(action);
    sig.bind_dirty(&decltype(eff)::on_dependency_dirty, &eff);

    EXPECT_EQ(effect_runs, 0);
    sig.set(2);
    EXPECT_EQ(effect_runs, 1);
    sig.set(3);
    EXPECT_EQ(effect_runs, 2);
}

} // namespace north::core::testing

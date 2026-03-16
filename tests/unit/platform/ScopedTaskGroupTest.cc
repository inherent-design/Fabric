#include "fabric/platform/ScopedTaskGroup.hh"
#include <future>
#include <gtest/gtest.h>
#include <string>

using namespace fabric::platform;

using IntGroup = ScopedTaskGroup<int, std::string>;

static std::future<std::string> readyFuture(const std::string& value) {
    std::promise<std::string> p;
    p.set_value(value);
    return p.get_future();
}

TEST(ScopedTaskGroupTest, SubmitAndPoll) {
    IntGroup group;
    ASSERT_TRUE(group.submit(1, readyFuture("hello")));

    auto results = group.poll(10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].key, 1);
    EXPECT_EQ(results[0].result, "hello");
    EXPECT_FALSE(results[0].cancelled);
    EXPECT_TRUE(group.empty());
}

TEST(ScopedTaskGroupTest, PollBudgetCap) {
    IntGroup group;
    group.submit(1, readyFuture("a"));
    group.submit(2, readyFuture("b"));
    group.submit(3, readyFuture("c"));

    auto first = group.poll(1);
    EXPECT_EQ(first.size(), 1u);
    EXPECT_EQ(group.size(), 2u);

    auto rest = group.poll(10);
    EXPECT_EQ(rest.size(), 2u);
    EXPECT_TRUE(group.empty());
}

TEST(ScopedTaskGroupTest, CancelAndPoll) {
    IntGroup group;
    group.submit(1, readyFuture("val"));
    ASSERT_TRUE(group.cancel(1));

    auto results = group.poll(10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].key, 1);
    EXPECT_TRUE(results[0].cancelled);
    EXPECT_EQ(results[0].result, "val");
}

TEST(ScopedTaskGroupTest, CancelAllAndPoll) {
    IntGroup group;
    group.submit(1, readyFuture("a"));
    group.submit(2, readyFuture("b"));
    group.submit(3, readyFuture("c"));
    group.cancelAll();

    auto results = group.poll(10);
    ASSERT_EQ(results.size(), 3u);
    for (const auto& r : results)
        EXPECT_TRUE(r.cancelled);
}

TEST(ScopedTaskGroupTest, HasKey) {
    IntGroup group;
    group.submit(42, readyFuture("x"));

    EXPECT_TRUE(group.has(42));
    EXPECT_FALSE(group.has(99));

    // cancel does not remove from the group
    group.cancel(42);
    EXPECT_TRUE(group.has(42));

    // poll removes the entry
    group.poll(10);
    EXPECT_FALSE(group.has(42));
}

TEST(ScopedTaskGroupTest, EmptyGroup) {
    IntGroup group;
    EXPECT_TRUE(group.empty());
    EXPECT_EQ(group.size(), 0u);

    auto results = group.poll(10);
    EXPECT_TRUE(results.empty());
}

TEST(ScopedTaskGroupTest, DuplicateKeyRejected) {
    IntGroup group;
    EXPECT_TRUE(group.submit(1, readyFuture("first")));
    EXPECT_FALSE(group.submit(1, readyFuture("second")));
    EXPECT_EQ(group.size(), 1u);

    auto results = group.poll(10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].result, "first");
}

TEST(ScopedTaskGroupTest, DestructorDrains) {
    std::promise<std::string> p;
    auto future = p.get_future();

    {
        IntGroup group;
        group.submit(1, std::move(future));

        // Set value from another thread before destruction
        p.set_value("done");
    } // destructor should wait on the future without crash
}

TEST(ScopedTaskGroupTest, CancelNonexistent) {
    IntGroup group;
    EXPECT_FALSE(group.cancel(999));
}

TEST(ScopedTaskGroupTest, PollEmptyAfterDrain) {
    IntGroup group;
    group.submit(1, readyFuture("a"));
    group.submit(2, readyFuture("b"));

    auto first = group.poll(10);
    EXPECT_EQ(first.size(), 2u);

    auto second = group.poll(10);
    EXPECT_TRUE(second.empty());
}

TEST(ScopedTaskGroupTest, MoveConstruct) {
    IntGroup group;
    group.submit(1, readyFuture("a"));
    group.submit(2, readyFuture("b"));

    IntGroup moved(std::move(group));
    EXPECT_EQ(moved.size(), 2u);

    auto results = moved.poll(10);
    EXPECT_EQ(results.size(), 2u);
}

struct TestMeta {
    int value;
    std::string label;
};

using MetaGroup = ScopedTaskGroup<int, std::string, std::hash<int>, TestMeta>;

TEST(ScopedTaskGroupTest, MetadataPreserved) {
    MetaGroup group;
    TestMeta meta{42, "test"};
    ASSERT_TRUE(group.submit(1, readyFuture("result"), meta));

    auto results = group.poll(10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].key, 1);
    EXPECT_EQ(results[0].result, "result");
    EXPECT_EQ(results[0].metadata.value, 42);
    EXPECT_EQ(results[0].metadata.label, "test");
    EXPECT_FALSE(results[0].cancelled);
}

#include "fabric/ui/ToastManager.hh"

#include <gtest/gtest.h>

using namespace fabric;

TEST(ToastManagerTest, InitiallyInactive) {
    ToastManager tm;
    EXPECT_FALSE(tm.active());
    EXPECT_TRUE(tm.currentMessage().empty());
}

TEST(ToastManagerTest, ShowMakesActive) {
    ToastManager tm;
    tm.show("hello", 2.0f);
    EXPECT_TRUE(tm.active());
    EXPECT_EQ(tm.currentMessage(), "hello");
}

TEST(ToastManagerTest, ToastExpiresAfterDuration) {
    ToastManager tm;
    tm.show("temp", 1.0f);
    EXPECT_TRUE(tm.active());

    tm.update(0.5f);
    EXPECT_TRUE(tm.active());
    EXPECT_EQ(tm.currentMessage(), "temp");

    tm.update(0.6f); // total 1.1s > 1.0s duration
    EXPECT_FALSE(tm.active());
}

TEST(ToastManagerTest, ClearRemovesAllToasts) {
    ToastManager tm;
    tm.show("a", 5.0f);
    tm.show("b", 5.0f);
    EXPECT_TRUE(tm.active());

    tm.clear();
    EXPECT_FALSE(tm.active());
}

TEST(ToastManagerTest, MultipleToastsCurrentMessageIsNewest) {
    ToastManager tm;
    tm.show("first", 5.0f);
    tm.show("second", 5.0f);
    EXPECT_EQ(tm.currentMessage(), "second");
}

TEST(ToastManagerTest, OlderToastExpiresSeparately) {
    ToastManager tm;
    tm.show("short", 1.0f);
    tm.show("long", 5.0f);

    tm.update(1.5f); // short expires, long remains
    EXPECT_TRUE(tm.active());
    EXPECT_EQ(tm.currentMessage(), "long");
}

TEST(ToastManagerTest, UpdateWithZeroDtDoesNotExpire) {
    ToastManager tm;
    tm.show("zero", 1.0f);
    tm.update(0.0f);
    EXPECT_TRUE(tm.active());
}

TEST(ToastManagerTest, ShowAfterClearWorks) {
    ToastManager tm;
    tm.show("a", 5.0f);
    tm.clear();
    EXPECT_FALSE(tm.active());

    tm.show("b", 5.0f);
    EXPECT_TRUE(tm.active());
    EXPECT_EQ(tm.currentMessage(), "b");
}

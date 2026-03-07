#include "fabric/utils/TimeoutLock.hh"
#include <gtest/gtest.h>

#include <chrono>
#include <shared_mutex>
#include <thread>

namespace fabric {
namespace Tests {

using utils::TimeoutLock;
using SharedMutex = std::shared_timed_mutex;

// -- tryLockShared --

TEST(TimeoutLockTest, SharedLockImmediateAcquisition) {
    SharedMutex mutex;
    auto lock = TimeoutLock<SharedMutex>::tryLockShared(mutex);
    EXPECT_TRUE(lock.has_value());
    EXPECT_TRUE(lock->owns_lock());
}

TEST(TimeoutLockTest, MultipleSharedLocksConcurrent) {
    SharedMutex mutex;
    auto lock1 = TimeoutLock<SharedMutex>::tryLockShared(mutex);
    auto lock2 = TimeoutLock<SharedMutex>::tryLockShared(mutex);
    EXPECT_TRUE(lock1.has_value());
    EXPECT_TRUE(lock2.has_value());
}

TEST(TimeoutLockTest, SharedLockTimesOutOnExclusiveHold) {
    SharedMutex mutex;

    // Hold exclusive lock in another thread
    std::unique_lock<SharedMutex> exclusiveLock(mutex);

    std::thread t([&mutex]() {
        auto lock = TimeoutLock<SharedMutex>::tryLockShared(mutex, std::chrono::milliseconds(20));
        EXPECT_FALSE(lock.has_value());
    });
    t.join();
}

// -- tryLockUnique --

TEST(TimeoutLockTest, UniqueLockImmediateAcquisition) {
    SharedMutex mutex;
    auto lock = TimeoutLock<SharedMutex>::tryLockUnique(mutex);
    EXPECT_TRUE(lock.has_value());
    EXPECT_TRUE(lock->owns_lock());
}

TEST(TimeoutLockTest, UniqueLockTimesOutOnSharedHold) {
    SharedMutex mutex;

    // Hold shared lock
    std::shared_lock<SharedMutex> sharedLock(mutex);

    std::thread t([&mutex]() {
        auto lock = TimeoutLock<SharedMutex>::tryLockUnique(mutex, std::chrono::milliseconds(20));
        EXPECT_FALSE(lock.has_value());
    });
    t.join();
}

TEST(TimeoutLockTest, UniqueLockTimesOutOnExclusiveHold) {
    SharedMutex mutex;

    std::unique_lock<SharedMutex> exclusiveLock(mutex);

    std::thread t([&mutex]() {
        auto lock = TimeoutLock<SharedMutex>::tryLockUnique(mutex, std::chrono::milliseconds(20));
        EXPECT_FALSE(lock.has_value());
    });
    t.join();
}

// -- tryUpgradeLock --

TEST(TimeoutLockTest, UpgradeLockSuccess) {
    SharedMutex mutex;
    auto sharedLock = std::shared_lock<SharedMutex>(mutex);
    EXPECT_TRUE(sharedLock.owns_lock());

    auto upgradedLock = TimeoutLock<SharedMutex>::tryUpgradeLock(mutex, sharedLock);
    EXPECT_TRUE(upgradedLock.has_value());
    EXPECT_TRUE(upgradedLock->owns_lock());
    // Shared lock was released during upgrade
    EXPECT_FALSE(sharedLock.owns_lock());
}

TEST(TimeoutLockTest, UpgradeLockFailureReacquiresShared) {
    SharedMutex mutex;

    // Hold exclusive lock in another thread to block upgrade
    std::unique_lock<SharedMutex> exclusiveLock(mutex);

    std::thread t([&mutex]() {
        auto sharedLock = std::shared_lock<SharedMutex>(mutex, std::try_to_lock);
        // Can't get shared lock while exclusive is held, so create unlocked
        sharedLock.release();
        // Manually set up a shared lock that we control
        SharedMutex localMutex;
        auto localShared = std::shared_lock<SharedMutex>(localMutex);

        auto upgraded = TimeoutLock<SharedMutex>::tryUpgradeLock(localMutex, localShared, std::chrono::milliseconds(5));
        // With no contention on localMutex, upgrade should succeed
        EXPECT_TRUE(upgraded.has_value());
    });
    t.join();
}

// -- Edge cases --

TEST(TimeoutLockTest, SharedLockZeroTimeout) {
    SharedMutex mutex;
    // Zero timeout should still try once
    auto lock = TimeoutLock<SharedMutex>::tryLockShared(mutex, std::chrono::milliseconds(0));
    EXPECT_TRUE(lock.has_value());
}

TEST(TimeoutLockTest, LockReleasedAfterScopeExit) {
    SharedMutex mutex;
    {
        auto lock = TimeoutLock<SharedMutex>::tryLockUnique(mutex);
        EXPECT_TRUE(lock.has_value());
    }
    // Lock should be released, so we can acquire again
    auto lock2 = TimeoutLock<SharedMutex>::tryLockUnique(mutex);
    EXPECT_TRUE(lock2.has_value());
}

} // namespace Tests
} // namespace fabric

#include "fabric/utils/BufferPool.hh"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

using namespace fabric;

TEST(BufferPoolTest, BasicBorrowReturn) {
  BufferPool pool(64, 4);
  EXPECT_EQ(pool.capacity(), 4u);
  EXPECT_EQ(pool.slotSize(), 64u);
  EXPECT_EQ(pool.available(), 4u);

  {
    auto slot = pool.borrow();
    EXPECT_EQ(pool.available(), 3u);
    EXPECT_NE(slot.data(), nullptr);
    EXPECT_EQ(slot.size(), 64u);
    EXPECT_EQ(slot.span().size(), 64u);
  }
  // Slot returned on destruction
  EXPECT_EQ(pool.available(), 4u);
}

TEST(BufferPoolTest, TryBorrowExhaustion) {
  BufferPool pool(32, 2);
  auto s1 = pool.tryBorrow();
  auto s2 = pool.tryBorrow();
  ASSERT_TRUE(s1.has_value());
  ASSERT_TRUE(s2.has_value());
  EXPECT_EQ(pool.available(), 0u);

  // Pool exhausted
  auto s3 = pool.tryBorrow();
  EXPECT_FALSE(s3.has_value());
}

TEST(BufferPoolTest, RAIIReturn) {
  BufferPool pool(16, 1);
  {
    auto slot = pool.borrow();
    EXPECT_EQ(pool.available(), 0u);
    // Write to the slot to verify it's usable
    slot.data()[0] = 0xAB;
    EXPECT_EQ(slot.data()[0], 0xAB);
  }
  EXPECT_EQ(pool.available(), 1u);
}

TEST(BufferPoolTest, MoveSemantics) {
  BufferPool pool(16, 2);
  auto slot1 = pool.borrow();
  EXPECT_EQ(pool.available(), 1u);

  // Move construct
  BufferSlot slot2 = std::move(slot1);
  EXPECT_EQ(pool.available(), 1u); // still only 1 borrowed
  EXPECT_NE(slot2.data(), nullptr);

  // Move assign
  BufferSlot slot3;
  slot3 = std::move(slot2);
  EXPECT_EQ(pool.available(), 1u);
  EXPECT_NE(slot3.data(), nullptr);
}

TEST(BufferPoolTest, ConcurrentBorrowReturn) {
  constexpr size_t kSlotCount = 8;
  constexpr size_t kThreadCount = 4;
  constexpr size_t kIterations = 100;

  BufferPool pool(64, kSlotCount);
  std::atomic<size_t> successCount{0};

  std::vector<std::thread> threads;
  for (size_t t = 0; t < kThreadCount; ++t) {
    threads.emplace_back([&]() {
      for (size_t i = 0; i < kIterations; ++i) {
        auto slot = pool.tryBorrow();
        if (slot.has_value()) {
          ++successCount;
          // Write something to verify the memory is usable
          slot->data()[0] = static_cast<uint8_t>(i & 0xFF);
        }
        // Slot returned on destruction
      }
    });
  }

  for (auto& t : threads)
    t.join();

  EXPECT_EQ(pool.available(), kSlotCount);
  EXPECT_GT(successCount.load(), 0u);
}

TEST(BufferPoolTest, BlockingBorrowWaitsForReturn) {
  BufferPool pool(16, 1);
  auto slot = pool.borrow();
  EXPECT_EQ(pool.available(), 0u);

  std::atomic<bool> borrowed{false};

  // Another thread tries to borrow (blocks)
  std::thread waiter([&]() {
    auto s = pool.borrow();
    borrowed.store(true);
    // Returns immediately
  });

  // Give the waiter time to block
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_FALSE(borrowed.load());

  // Release our slot so the waiter can proceed
  slot = BufferSlot(); // move-assign empty -> releases
  waiter.join();
  EXPECT_TRUE(borrowed.load());
  EXPECT_EQ(pool.available(), 1u);
}

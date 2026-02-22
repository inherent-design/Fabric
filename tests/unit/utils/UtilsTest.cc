#include "fabric/utils/Utils.hh"
#include "gtest/gtest.h"
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

class UtilsTest : public ::testing::Test {};

TEST_F(UtilsTest, TestGenerateUniqueId) {
  std::string id1 = fabric::Utils::generateUniqueId("test_");
  std::string id2 = fabric::Utils::generateUniqueId("test_");

  ASSERT_FALSE(id1.empty());
  ASSERT_FALSE(id2.empty());
  ASSERT_TRUE(id1.starts_with("test_"));
  ASSERT_TRUE(id2.starts_with("test_"));
  ASSERT_NE(id1, id2);

  std::string id3 = fabric::Utils::generateUniqueId("prefix_", 4);
  ASSERT_EQ(id3.length(), 11); // "prefix_" (7) + 4 hex digits
}

TEST_F(UtilsTest, TestGenerateUniqueIdThreadSafety) {
  const int numThreads = 10;
  const int idsPerThread = 100;
  std::unordered_set<std::string> generatedIds;
  std::mutex idsMutex;

  auto generateIdsTask = [&]() {
    std::vector<std::string> threadIds;
    threadIds.reserve(idsPerThread);

    for (int i = 0; i < idsPerThread; i++) {
      threadIds.push_back(fabric::Utils::generateUniqueId("thread_"));
    }

    std::lock_guard<std::mutex> lock(idsMutex);
    for (const auto& id : threadIds) {
      generatedIds.insert(id);
    }
  };

  std::vector<std::future<void>> futures;
  for (int i = 0; i < numThreads; i++) {
    futures.push_back(std::async(std::launch::async, generateIdsTask));
  }

  for (auto& future : futures) {
    future.wait();
  }

  ASSERT_EQ(generatedIds.size(), numThreads * idsPerThread);
}

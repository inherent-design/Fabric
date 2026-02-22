#pragma once

#include "fabric/core/Component.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Lifecycle.hh"
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>
#include <future>
#include <vector>
#include <stdexcept>
#include <random>

namespace fabric {
namespace Testing {

/**
 * @brief Mock component for testing
 */
class MockComponent : public Component {
public:
  explicit MockComponent(const std::string& id) : Component(id) {}

  void initialize() override { initialize_impl(); }
  std::string render() override { render_impl(); return "<mock-component id=\"" + getId() + "\"></mock-component>"; }
  void update(float deltaTime) override { update_impl(deltaTime); }
  void cleanup() override { cleanup_impl(); }

  // Test helpers
  int initializeCallCount = 0;
  int renderCallCount = 0;
  int updateCallCount = 0;
  int cleanupCallCount = 0;

  // Override these methods for testing
  void initialize_impl() { initializeCallCount++; }
  std::string render_impl() { renderCallCount++; return ""; }
  void update_impl(float deltaTime) { updateCallCount++; }
  void cleanup_impl() { cleanupCallCount++; }
};

/**
 * @brief Event recorder for testing event dispatch
 */
class EventRecorder {
public:
  /**
   * @brief Record an event
   * 
   * @param event The event to record
   */
  void recordEvent(const Event& event) {
    lastEventType = event.getType();
    lastEventSource = event.getSource();
    eventCount++;
  }

  /**
   * @brief Get a handler function for the event recorder
   * 
   * @return Event handler function
   */
  EventHandler getHandler() {
    return [this](const Event& event) {
      this->recordEvent(event);
    };
  }

  /**
   * @brief Reset the recorder
   */
  void reset() {
    lastEventType = "";
    lastEventSource = "";
    eventCount = 0;
  }

  // Record data
  std::string lastEventType;
  std::string lastEventSource;
  int eventCount = 0;
};

/**
 * @brief Lifecycle recorder for testing lifecycle transitions
 */
class LifecycleRecorder {
public:
  /**
   * @brief Record a lifecycle state change
   * 
   * @param state The new state
   */
  void recordState(LifecycleState state) {
    lastState = state;
    stateChanges++;
  }

  /**
   * @brief Get a handler function for the lifecycle recorder
   * 
   * @return Lifecycle hook function
   */
  LifecycleHook getHook() {
    return [this]() {
      this->stateChanges++;
    };
  }

  /**
   * @brief Get a transition handler function for the lifecycle recorder
   * 
   * @param fromState The from state
   * @param toState The to state
   * @return Lifecycle hook function
   */
  LifecycleHook getTransitionHook(LifecycleState fromState, LifecycleState toState) {
    return [this, fromState, toState]() {
      this->lastFromState = fromState;
      this->lastToState = toState;
      this->transitionChanges++;
    };
  }

  /**
   * @brief Reset the recorder
   */
  void reset() {
    lastState = LifecycleState::Created;
    lastFromState = LifecycleState::Created;
    lastToState = LifecycleState::Created;
    stateChanges = 0;
    transitionChanges = 0;
  }

  // Record data
  LifecycleState lastState = LifecycleState::Created;
  LifecycleState lastFromState = LifecycleState::Created;
  LifecycleState lastToState = LifecycleState::Created;
  int stateChanges = 0;
  int transitionChanges = 0;
};

/**
 * @brief Run a function with a timeout
 * 
 * @tparam Func Function type
 * @param func Function to run
 * @param timeout Timeout duration
 * @return true if the function completed before the timeout, false otherwise
 */
template<typename Func>
bool RunWithTimeout(Func&& func, std::chrono::milliseconds timeout) {
    std::atomic<bool> completed{false};
    
    std::thread t([&]() {
        func();
        completed = true;
    });
    
    // Wait for completion or timeout
    auto start = std::chrono::steady_clock::now();
    while (!completed) {
        auto now = std::chrono::steady_clock::now();
        if (now - start > timeout) {
            t.detach();  // Don't join the thread if it's still running
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    t.join();
    return true;
}

/**
 * @brief Run a function with multiple concurrent threads
 * 
 * @param threadCount Number of threads to spawn
 * @param iterationsPerThread Number of iterations per thread
 * @param func Function to run in each iteration
 * @throws std::runtime_error if any thread throws an exception
 */
inline void RunConcurrent(
    size_t threadCount,
    size_t iterationsPerThread,
    std::function<void(size_t threadId, size_t iteration)> func
) {
    std::vector<std::future<void>> futures;
    futures.reserve(threadCount);
    
    for (size_t threadId = 0; threadId < threadCount; ++threadId) {
        futures.push_back(std::async(std::launch::async, [=]() {
            for (size_t i = 0; i < iterationsPerThread; ++i) {
                func(threadId, i);
            }
        }));
    }
    
    // Wait for all threads to complete and propagate exceptions
    for (auto& future : futures) {
        future.get();
    }
}

/**
 * @brief Generate a random string of the specified length
 * 
 * @param length Length of the string to generate
 * @return Random string
 */
inline std::string RandomString(size_t length) {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[distribution(generator)];
    }
    
    return result;
}

/**
 * @brief Generate a random integer within the specified range
 * 
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return Random integer
 */
inline int RandomInt(int min, int max) {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(generator);
}

} // namespace Testing
} // namespace fabric
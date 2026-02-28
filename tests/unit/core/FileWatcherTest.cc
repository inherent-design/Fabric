#include "fabric/core/FileWatcher.hh"
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

using namespace fabric;

class FileWatcherTest : public ::testing::Test {
  protected:
    FileWatcher watcher;
};

TEST_F(FileWatcherTest, NotValidBeforeInit) {
    EXPECT_FALSE(watcher.isValid());
}

TEST_F(FileWatcherTest, InitMakesValid) {
    watcher.init();
    EXPECT_TRUE(watcher.isValid());
    watcher.shutdown();
}

TEST_F(FileWatcherTest, ShutdownMakesInvalid) {
    watcher.init();
    watcher.shutdown();
    EXPECT_FALSE(watcher.isValid());
}

TEST_F(FileWatcherTest, DoubleInitIsSafe) {
    watcher.init();
    watcher.init();
    EXPECT_TRUE(watcher.isValid());
    watcher.shutdown();
}

TEST_F(FileWatcherTest, DoubleShutdownIsSafe) {
    watcher.init();
    watcher.shutdown();
    watcher.shutdown();
    EXPECT_FALSE(watcher.isValid());
}

TEST_F(FileWatcherTest, ShutdownBeforeInitIsSafe) {
    watcher.shutdown();
    EXPECT_FALSE(watcher.isValid());
}

TEST_F(FileWatcherTest, RegisterAndUnregisterResource) {
    watcher.init();

    bool swapCalled = false;
    watcher.registerResource(
        "/tmp/test.glsl", [](const std::string&) { return true; },
        [&swapCalled](const std::string&) { swapCalled = true; });

    watcher.unregisterResource("/tmp/test.glsl");
    watcher.shutdown();
}

TEST_F(FileWatcherTest, UnregisterNonexistentResourceIsSafe) {
    watcher.init();
    watcher.unregisterResource("/nonexistent/path");
    watcher.shutdown();
}

TEST_F(FileWatcherTest, PollWithoutInitIsSafe) {
    watcher.poll();
}

TEST_F(FileWatcherTest, PollWithNoEventsIsNoOp) {
    watcher.init();
    watcher.poll();
    watcher.shutdown();
}

TEST_F(FileWatcherTest, ExtensionFilterAcceptsMatchingExtension) {
    watcher.init();
    watcher.setExtensionFilter({".glsl", ".so"});

    bool swapCalled = false;
    watcher.registerResource(
        "/tmp/shader.glsl", [](const std::string&) { return true; },
        [&swapCalled](const std::string&) { swapCalled = true; });

    // Simulate a matching event
    FileChangeEvent event;
    event.directory = "/tmp";
    event.filename = "shader.glsl";
    event.fullPath = "/tmp/shader.glsl";
    event.timestamp = std::chrono::steady_clock::now();
    watcher.enqueueEvent(event);

    watcher.poll();
    EXPECT_TRUE(swapCalled);
    watcher.shutdown();
}

TEST_F(FileWatcherTest, ExtensionFilterRejectsNonMatchingExtension) {
    watcher.init();
    watcher.setExtensionFilter({".glsl"});

    bool swapCalled = false;
    watcher.registerResource(
        "/tmp/data.json", [](const std::string&) { return true; },
        [&swapCalled](const std::string&) { swapCalled = true; });

    FileChangeEvent event;
    event.directory = "/tmp";
    event.filename = "data.json";
    event.fullPath = "/tmp/data.json";
    event.timestamp = std::chrono::steady_clock::now();
    watcher.enqueueEvent(event);

    watcher.poll();
    EXPECT_FALSE(swapCalled);
    watcher.shutdown();
}

TEST_F(FileWatcherTest, ExtensionFilterNormalizesWithoutDot) {
    watcher.init();
    watcher.setExtensionFilter({"glsl"}); // No leading dot

    bool swapCalled = false;
    watcher.registerResource(
        "/tmp/shader.glsl", [](const std::string&) { return true; },
        [&swapCalled](const std::string&) { swapCalled = true; });

    FileChangeEvent event;
    event.directory = "/tmp";
    event.filename = "shader.glsl";
    event.fullPath = "/tmp/shader.glsl";
    event.timestamp = std::chrono::steady_clock::now();
    watcher.enqueueEvent(event);

    watcher.poll();
    EXPECT_TRUE(swapCalled);
    watcher.shutdown();
}

TEST_F(FileWatcherTest, NoFilterAcceptsAll) {
    watcher.init();
    // No extension filter set

    bool swapCalled = false;
    watcher.registerResource(
        "/tmp/anything.xyz", [](const std::string&) { return true; },
        [&swapCalled](const std::string&) { swapCalled = true; });

    FileChangeEvent event;
    event.directory = "/tmp";
    event.filename = "anything.xyz";
    event.fullPath = "/tmp/anything.xyz";
    event.timestamp = std::chrono::steady_clock::now();
    watcher.enqueueEvent(event);

    watcher.poll();
    EXPECT_TRUE(swapCalled);
    watcher.shutdown();
}

TEST_F(FileWatcherTest, DebounceCollapsesRapidEvents) {
    watcher.init();

    int swapCount = 0;
    watcher.registerResource(
        "/tmp/shader.glsl", [](const std::string&) { return true; }, [&swapCount](const std::string&) { swapCount++; });

    auto now = std::chrono::steady_clock::now();

    // Two events within 100ms debounce window
    FileChangeEvent event1;
    event1.directory = "/tmp";
    event1.filename = "shader.glsl";
    event1.fullPath = "/tmp/shader.glsl";
    event1.timestamp = now;

    FileChangeEvent event2;
    event2.directory = "/tmp";
    event2.filename = "shader.glsl";
    event2.fullPath = "/tmp/shader.glsl";
    event2.timestamp = now + std::chrono::milliseconds(10); // 10ms later

    watcher.enqueueEvent(event1);
    watcher.enqueueEvent(event2);

    watcher.poll();
    EXPECT_EQ(swapCount, 1); // Second event debounced

    watcher.shutdown();
}

TEST_F(FileWatcherTest, DebounceAllowsEventsAfterWindow) {
    watcher.init();

    int swapCount = 0;
    watcher.registerResource(
        "/tmp/shader.glsl", [](const std::string&) { return true; }, [&swapCount](const std::string&) { swapCount++; });

    auto now = std::chrono::steady_clock::now();

    // First event
    FileChangeEvent event1;
    event1.directory = "/tmp";
    event1.filename = "shader.glsl";
    event1.fullPath = "/tmp/shader.glsl";
    event1.timestamp = now;

    watcher.enqueueEvent(event1);
    watcher.poll();
    EXPECT_EQ(swapCount, 1);

    // Second event after debounce window
    FileChangeEvent event2;
    event2.directory = "/tmp";
    event2.filename = "shader.glsl";
    event2.fullPath = "/tmp/shader.glsl";
    event2.timestamp = now + std::chrono::milliseconds(200); // 200ms later

    watcher.enqueueEvent(event2);
    watcher.poll();
    EXPECT_EQ(swapCount, 2); // Both events processed

    watcher.shutdown();
}

TEST_F(FileWatcherTest, ValidationFailurePreventsSwap) {
    watcher.init();

    bool swapCalled = false;
    watcher.registerResource(
        "/tmp/bad.glsl", [](const std::string&) { return false; }, // Validation always fails
        [&swapCalled](const std::string&) { swapCalled = true; });

    FileChangeEvent event;
    event.directory = "/tmp";
    event.filename = "bad.glsl";
    event.fullPath = "/tmp/bad.glsl";
    event.timestamp = std::chrono::steady_clock::now();
    watcher.enqueueEvent(event);

    watcher.poll();
    EXPECT_FALSE(swapCalled);
    watcher.shutdown();
}

TEST_F(FileWatcherTest, NullValidateCallbackSkipsValidation) {
    watcher.init();

    bool swapCalled = false;
    watcher.registerResource("/tmp/shader.glsl", nullptr, // No validation
                             [&swapCalled](const std::string&) { swapCalled = true; });

    FileChangeEvent event;
    event.directory = "/tmp";
    event.filename = "shader.glsl";
    event.fullPath = "/tmp/shader.glsl";
    event.timestamp = std::chrono::steady_clock::now();
    watcher.enqueueEvent(event);

    watcher.poll();
    EXPECT_TRUE(swapCalled);
    watcher.shutdown();
}

TEST_F(FileWatcherTest, UnregisteredPathIgnored) {
    watcher.init();

    // No resource registered for this path
    FileChangeEvent event;
    event.directory = "/tmp";
    event.filename = "unknown.txt";
    event.fullPath = "/tmp/unknown.txt";
    event.timestamp = std::chrono::steady_clock::now();
    watcher.enqueueEvent(event);

    // Should not crash
    watcher.poll();
    watcher.shutdown();
}

TEST_F(FileWatcherTest, WatchDirectoryBeforeInitWarns) {
    // Should log warning but not crash
    watcher.watchDirectory("/tmp");
}

TEST_F(FileWatcherTest, MultipleResourcesIndependent) {
    watcher.init();

    bool swap1Called = false;
    bool swap2Called = false;

    watcher.registerResource(
        "/tmp/a.glsl", [](const std::string&) { return true; },
        [&swap1Called](const std::string&) { swap1Called = true; });
    watcher.registerResource(
        "/tmp/b.glsl", [](const std::string&) { return true; },
        [&swap2Called](const std::string&) { swap2Called = true; });

    // Only trigger event for a.glsl
    FileChangeEvent event;
    event.directory = "/tmp";
    event.filename = "a.glsl";
    event.fullPath = "/tmp/a.glsl";
    event.timestamp = std::chrono::steady_clock::now();
    watcher.enqueueEvent(event);

    watcher.poll();
    EXPECT_TRUE(swap1Called);
    EXPECT_FALSE(swap2Called);

    watcher.shutdown();
}

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "fabric/core/AppContext.hh"
#include "fabric/core/FabricApp.hh"

namespace fabric {

// Records lifecycle events for verification
static std::vector<std::string> lifecycleLog;

class RecorderSystemA : public System<RecorderSystemA> {
  public:
    void init(AppContext& /*ctx*/) override { lifecycleLog.push_back("A::init"); }
    void shutdown() override { lifecycleLog.push_back("A::shutdown"); }
};

class RecorderSystemB : public System<RecorderSystemB> {
  public:
    void init(AppContext& /*ctx*/) override { lifecycleLog.push_back("B::init"); }
    void shutdown() override { lifecycleLog.push_back("B::shutdown"); }
};

class FabricAppTest : public ::testing::Test {
  protected:
    void SetUp() override { lifecycleLog.clear(); }
};

TEST_F(FabricAppTest, LifecyclePhaseOrdering) {
    FabricAppDesc desc;
    desc.name = "TestApp";
    desc.registerSystem<RecorderSystemA>(SystemPhase::Update);
    desc.registerSystem<RecorderSystemB>(SystemPhase::PostUpdate);

    desc.onInit = [](AppContext& /*ctx*/) {
        lifecycleLog.push_back("onInit");
    };
    desc.onShutdown = [](AppContext& /*ctx*/) {
        lifecycleLog.push_back("onShutdown");
    };

    char arg0[] = "test";
    char* args[] = {arg0};
    int result = FabricApp::run(1, args, std::move(desc));

    EXPECT_EQ(result, 0);

    // Verify ordering: system init -> onInit -> onShutdown -> system shutdown (reverse)
    ASSERT_GE(lifecycleLog.size(), 6u);

    // Systems init in resolved order
    auto initA = std::find(lifecycleLog.begin(), lifecycleLog.end(), "A::init");
    auto initB = std::find(lifecycleLog.begin(), lifecycleLog.end(), "B::init");
    auto onInit = std::find(lifecycleLog.begin(), lifecycleLog.end(), "onInit");
    auto onShutdown = std::find(lifecycleLog.begin(), lifecycleLog.end(), "onShutdown");
    auto shutdownA = std::find(lifecycleLog.begin(), lifecycleLog.end(), "A::shutdown");
    auto shutdownB = std::find(lifecycleLog.begin(), lifecycleLog.end(), "B::shutdown");

    ASSERT_NE(initA, lifecycleLog.end());
    ASSERT_NE(initB, lifecycleLog.end());
    ASSERT_NE(onInit, lifecycleLog.end());
    ASSERT_NE(onShutdown, lifecycleLog.end());
    ASSERT_NE(shutdownA, lifecycleLog.end());
    ASSERT_NE(shutdownB, lifecycleLog.end());

    // System inits before onInit
    EXPECT_LT(initA, onInit);
    EXPECT_LT(initB, onInit);

    // onInit before onShutdown
    EXPECT_LT(onInit, onShutdown);

    // onShutdown before system shutdowns
    EXPECT_LT(onShutdown, shutdownA);
    EXPECT_LT(onShutdown, shutdownB);
}

TEST_F(FabricAppTest, InitCallbackReceivesAppContext) {
    FabricAppDesc desc;
    desc.name = "ContextTest";

    bool gotContext = false;
    desc.onInit = [&](AppContext& ctx) {
        // Verify we can access required refs without crashing
        (void)ctx.configManager;
        (void)ctx.systemRegistry;
        (void)ctx.assetRegistry;
        (void)ctx.resourceHub;
        gotContext = true;
    };

    char arg0[] = "test";
    char* args[] = {arg0};
    FabricApp::run(1, args, std::move(desc));

    EXPECT_TRUE(gotContext);
}

TEST_F(FabricAppTest, NoCallbacksStillRuns) {
    FabricAppDesc desc;
    desc.name = "MinimalApp";

    char arg0[] = "test";
    char* args[] = {arg0};
    int result = FabricApp::run(1, args, std::move(desc));

    EXPECT_EQ(result, 0);
}

TEST_F(FabricAppTest, HelpFlagReturnsZero) {
    FabricAppDesc desc;
    desc.name = "HelpTest";

    char arg0[] = "test";
    char arg1[] = "--help";
    char* args[] = {arg0, arg1};
    int result = FabricApp::run(2, args, std::move(desc));

    EXPECT_EQ(result, 0);
}

} // namespace fabric

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/FabricApp.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemRegistry.hh"

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

TEST_F(FabricAppTest, VersionFlagReturnsZero) {
    FabricAppDesc desc;
    desc.name = "VersionTest";

    char arg0[] = "test";
    char arg1[] = "--version";
    char* args[] = {arg0, arg1};
    int result = FabricApp::run(2, args, std::move(desc));

    EXPECT_EQ(result, 0);
}

TEST_F(FabricAppTest, EmptyDescRunsCleanly) {
    FabricAppDesc desc;
    // No systems, no callbacks, no config

    char arg0[] = "test";
    char* args[] = {arg0};
    int result = FabricApp::run(1, args, std::move(desc));

    EXPECT_EQ(result, 0);
}

// System that creates a cycle for testing failure path
class CycleSystemR : public System<CycleSystemR> {
  public:
    void configureDependencies() override;
};

class CycleSystemS : public System<CycleSystemS> {
  public:
    void configureDependencies() override { after<CycleSystemR>(); }
};

void CycleSystemR::configureDependencies() {
    after<CycleSystemS>();
}

TEST_F(FabricAppTest, CyclicDependencyReturnsExitCodeOne) {
    FabricAppDesc desc;
    desc.name = "CycleApp";
    desc.registerSystem<CycleSystemR>(SystemPhase::Update);
    desc.registerSystem<CycleSystemS>(SystemPhase::Update);

    char arg0[] = "test";
    char* args[] = {arg0};
    int result = FabricApp::run(1, args, std::move(desc));

    EXPECT_EQ(result, 1);
}

// Verify systems init/shutdown in dependency order through FabricApp::run()
class DepOrderA : public System<DepOrderA> {
  public:
    void init(AppContext& /*ctx*/) override { lifecycleLog.push_back("DepA::init"); }
    void shutdown() override { lifecycleLog.push_back("DepA::shutdown"); }
};

class DepOrderB : public System<DepOrderB> {
  public:
    void configureDependencies() override { after<DepOrderA>(); }
    void init(AppContext& /*ctx*/) override { lifecycleLog.push_back("DepB::init"); }
    void shutdown() override { lifecycleLog.push_back("DepB::shutdown"); }
};

TEST_F(FabricAppTest, DependencyOrderedInitShutdownThroughRun) {
    FabricAppDesc desc;
    desc.name = "DepOrderApp";
    // Register B before A to ensure toposort handles registration order
    desc.registerSystem<DepOrderB>(SystemPhase::Update);
    desc.registerSystem<DepOrderA>(SystemPhase::Update);

    char arg0[] = "test";
    char* args[] = {arg0};
    int result = FabricApp::run(1, args, std::move(desc));

    EXPECT_EQ(result, 0);

    auto initA = std::find(lifecycleLog.begin(), lifecycleLog.end(), "DepA::init");
    auto initB = std::find(lifecycleLog.begin(), lifecycleLog.end(), "DepB::init");
    auto shutA = std::find(lifecycleLog.begin(), lifecycleLog.end(), "DepA::shutdown");
    auto shutB = std::find(lifecycleLog.begin(), lifecycleLog.end(), "DepB::shutdown");

    ASSERT_NE(initA, lifecycleLog.end());
    ASSERT_NE(initB, lifecycleLog.end());
    ASSERT_NE(shutA, lifecycleLog.end());
    ASSERT_NE(shutB, lifecycleLog.end());

    // A inits before B (dependency)
    EXPECT_LT(initA, initB);
    // B shuts down before A (reverse)
    EXPECT_LT(shutB, shutA);
}

// ── Init failure recovery: A inits ok, B throws, C never inits ────────

class InitOkSystem : public System<InitOkSystem> {
  public:
    void init(AppContext& /*ctx*/) override { lifecycleLog.push_back("InitOk::init"); }
    void shutdown() override { lifecycleLog.push_back("InitOk::shutdown"); }
};

class InitFailSystem : public System<InitFailSystem> {
  public:
    void configureDependencies() override { after<InitOkSystem>(); }
    void init(AppContext& /*ctx*/) override {
        lifecycleLog.push_back("InitFail::init");
        throw std::runtime_error("InitFailSystem blew up");
    }
    void shutdown() override { lifecycleLog.push_back("InitFail::shutdown"); }
};

class InitNeverSystem : public System<InitNeverSystem> {
  public:
    void configureDependencies() override { after<InitFailSystem>(); }
    void init(AppContext& /*ctx*/) override { lifecycleLog.push_back("InitNever::init"); }
    void shutdown() override { lifecycleLog.push_back("InitNever::shutdown"); }
};

TEST_F(FabricAppTest, InitFailureRecovery) {
    FabricAppDesc desc;
    desc.name = "InitFailApp";
    desc.registerSystem<InitOkSystem>(SystemPhase::Update);
    desc.registerSystem<InitFailSystem>(SystemPhase::Update);
    desc.registerSystem<InitNeverSystem>(SystemPhase::Update);

    char arg0[] = "test";
    char* args[] = {arg0};
    int result = FabricApp::run(1, args, std::move(desc));

    // run() should return non-zero on init failure
    EXPECT_NE(result, 0);

    // A (InitOk) initialized and was cleaned up
    EXPECT_NE(std::find(lifecycleLog.begin(), lifecycleLog.end(), "InitOk::init"), lifecycleLog.end());
    EXPECT_NE(std::find(lifecycleLog.begin(), lifecycleLog.end(), "InitOk::shutdown"), lifecycleLog.end());

    // B (InitFail) attempted init but should NOT be shut down (never completed init)
    EXPECT_NE(std::find(lifecycleLog.begin(), lifecycleLog.end(), "InitFail::init"), lifecycleLog.end());
    EXPECT_EQ(std::find(lifecycleLog.begin(), lifecycleLog.end(), "InitFail::shutdown"), lifecycleLog.end());

    // C (InitNever) should never have init() or shutdown() called
    EXPECT_EQ(std::find(lifecycleLog.begin(), lifecycleLog.end(), "InitNever::init"), lifecycleLog.end());
    EXPECT_EQ(std::find(lifecycleLog.begin(), lifecycleLog.end(), "InitNever::shutdown"), lifecycleLog.end());
}

// ── AppContext construction and optional member tests ──────────────────

TEST(AppContextTest, RequiredRefsAccessible) {
    World world;
    Timeline timeline;
    EventDispatcher dispatcher;
    ResourceHub hub;
    hub.disableWorkerThreadsForTesting();
    AssetRegistry assetRegistry(hub);
    SystemRegistry systemRegistry;
    ConfigManager configManager;

    AppContext ctx{
        .world = world,
        .timeline = timeline,
        .dispatcher = dispatcher,
        .resourceHub = hub,
        .assetRegistry = assetRegistry,
        .systemRegistry = systemRegistry,
        .configManager = configManager,
    };

    // All 7 required refs should be accessible without crashing
    EXPECT_EQ(&ctx.world, &world);
    EXPECT_EQ(&ctx.timeline, &timeline);
    EXPECT_EQ(&ctx.dispatcher, &dispatcher);
    EXPECT_EQ(&ctx.resourceHub, &hub);
    EXPECT_EQ(&ctx.assetRegistry, &assetRegistry);
    EXPECT_EQ(&ctx.systemRegistry, &systemRegistry);
    EXPECT_EQ(&ctx.configManager, &configManager);
}

TEST(AppContextTest, OptionalPtrsDefaultNull) {
    World world;
    Timeline timeline;
    EventDispatcher dispatcher;
    ResourceHub hub;
    hub.disableWorkerThreadsForTesting();
    AssetRegistry assetRegistry(hub);
    SystemRegistry systemRegistry;
    ConfigManager configManager;

    AppContext ctx{
        .world = world,
        .timeline = timeline,
        .dispatcher = dispatcher,
        .resourceHub = hub,
        .assetRegistry = assetRegistry,
        .systemRegistry = systemRegistry,
        .configManager = configManager,
    };

    EXPECT_EQ(ctx.inputSystem, nullptr);
    EXPECT_EQ(ctx.runtimeState, nullptr);
    EXPECT_EQ(ctx.platformInfo, nullptr);
    EXPECT_EQ(ctx.renderCaps, nullptr);
    EXPECT_EQ(ctx.appModeManager, nullptr);
    EXPECT_EQ(ctx.window, nullptr);
    EXPECT_EQ(ctx.cursorManager, nullptr);
    EXPECT_EQ(ctx.inputManager, nullptr);
    EXPECT_EQ(ctx.inputRouter, nullptr);
    EXPECT_EQ(ctx.camera, nullptr);
    EXPECT_EQ(ctx.sceneView, nullptr);
    EXPECT_EQ(ctx.rmlContext, nullptr);
}

TEST(AppContextTest, OptionalPtrNullCheckPattern) {
    World world;
    Timeline timeline;
    EventDispatcher dispatcher;
    ResourceHub hub;
    hub.disableWorkerThreadsForTesting();
    AssetRegistry assetRegistry(hub);
    SystemRegistry systemRegistry;
    ConfigManager configManager;

    AppContext ctx{
        .world = world,
        .timeline = timeline,
        .dispatcher = dispatcher,
        .resourceHub = hub,
        .assetRegistry = assetRegistry,
        .systemRegistry = systemRegistry,
        .configManager = configManager,
    };

    // Safe pattern: null check before access
    bool inputAvailable = (ctx.inputSystem != nullptr);
    EXPECT_FALSE(inputAvailable);

    bool runtimeAvailable = (ctx.runtimeState != nullptr);
    EXPECT_FALSE(runtimeAvailable);
}

// ── FabricAppDesc new fields ───────────────────────────────────────────

TEST(FabricAppDescTest, HeadlessDefaultTrue) {
    FabricAppDesc desc;
    EXPECT_TRUE(desc.headless);
}

TEST_F(FabricAppTest, HeadlessModeSkipsPlatform) {
    // headless=true (default) should run through lifecycle without SDL/bgfx
    FabricAppDesc desc;
    desc.name = "HeadlessTest";

    bool initCalled = false;
    bool shutdownCalled = false;
    desc.onInit = [&](AppContext& ctx) {
        initCalled = true;
        // In headless mode, platform pointers should be null
        EXPECT_EQ(ctx.window, nullptr);
        EXPECT_EQ(ctx.inputManager, nullptr);
        EXPECT_EQ(ctx.inputRouter, nullptr);
        EXPECT_EQ(ctx.camera, nullptr);
        EXPECT_EQ(ctx.sceneView, nullptr);
        EXPECT_EQ(ctx.rmlContext, nullptr);
    };
    desc.onShutdown = [&](AppContext& /*ctx*/) {
        shutdownCalled = true;
    };

    char arg0[] = "test";
    char* args[] = {arg0};
    int result = FabricApp::run(1, args, std::move(desc));

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(initCalled);
    EXPECT_TRUE(shutdownCalled);
}

} // namespace fabric

#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/AppContext.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>

using namespace fabric;

// Call log shared across mock systems for ordering verification
static std::vector<std::string>* g_callLog = nullptr;

namespace {

class MockA : public System<MockA> {
  public:
    int initCount = 0;
    int updateCount = 0;
    int fixedUpdateCount = 0;
    int renderCount = 0;
    int shutdownCount = 0;

    void init(AppContext& ctx) override {
        ++initCount;
        if (g_callLog)
            g_callLog->push_back("A::init");
    }
    void shutdown() override {
        ++shutdownCount;
        if (g_callLog)
            g_callLog->push_back("A::shutdown");
    }
    void update(AppContext& ctx, float dt) override {
        ++updateCount;
        if (g_callLog)
            g_callLog->push_back("A::update");
    }
    void fixedUpdate(AppContext& ctx, float fixedDt) override {
        ++fixedUpdateCount;
        if (g_callLog)
            g_callLog->push_back("A::fixedUpdate");
    }
    void render(AppContext& ctx) override {
        ++renderCount;
        if (g_callLog)
            g_callLog->push_back("A::render");
    }
};

class MockB : public System<MockB> {
  public:
    int initCount = 0;
    int updateCount = 0;
    int shutdownCount = 0;

    void configureDependencies() override { after<MockA>(); }

    void init(AppContext& ctx) override {
        ++initCount;
        if (g_callLog)
            g_callLog->push_back("B::init");
    }
    void shutdown() override {
        ++shutdownCount;
        if (g_callLog)
            g_callLog->push_back("B::shutdown");
    }
    void update(AppContext& ctx, float dt) override {
        ++updateCount;
        if (g_callLog)
            g_callLog->push_back("B::update");
    }
};

class MockC : public System<MockC> {
  public:
    int initCount = 0;
    int updateCount = 0;
    int shutdownCount = 0;

    void configureDependencies() override { after<MockB>(); }

    void init(AppContext& ctx) override {
        ++initCount;
        if (g_callLog)
            g_callLog->push_back("C::init");
    }
    void shutdown() override {
        ++shutdownCount;
        if (g_callLog)
            g_callLog->push_back("C::shutdown");
    }
    void update(AppContext& ctx, float dt) override {
        ++updateCount;
        if (g_callLog)
            g_callLog->push_back("C::update");
    }
};

// Uses before() instead of after()
class MockD : public System<MockD> {
  public:
    void configureDependencies() override { before<MockA>(); }

    void init(AppContext& ctx) override {
        if (g_callLog)
            g_callLog->push_back("D::init");
    }
    void shutdown() override {
        if (g_callLog)
            g_callLog->push_back("D::shutdown");
    }
};

// Creates a cycle: CycleX after CycleY, CycleY after CycleX
class CycleX : public System<CycleX> {
  public:
    void configureDependencies() override { after<CycleX>(); }
};

class CycleY : public System<CycleY> {};

// Two systems that form a real cycle
class CycleP : public System<CycleP> {
  public:
    void configureDependencies() override;
};

class CycleQ : public System<CycleQ> {
  public:
    void configureDependencies() override { after<CycleP>(); }
};

void CycleP::configureDependencies() {
    after<CycleQ>();
}

} // namespace

class SystemRegistryTest : public ::testing::Test {
  protected:
    World world;
    Timeline timeline;
    EventDispatcher dispatcher;
    ResourceHub hub;

    void SetUp() override {
        hub.disableWorkerThreadsForTesting();
        callLog.clear();
        g_callLog = &callLog;
    }

    void TearDown() override { g_callLog = nullptr; }

    AppContext makeContext() { return AppContext{world, timeline, dispatcher, hub}; }

    std::vector<std::string> callLog;
};

TEST_F(SystemRegistryTest, RegisterAndRetrieve) {
    SystemRegistry reg;
    reg.registerSystem<MockA>(SystemPhase::Update);
    EXPECT_TRUE(reg.resolve());

    auto* a = reg.get<MockA>();
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->initCount, 0);
}

TEST_F(SystemRegistryTest, RegisterDuplicateThrows) {
    SystemRegistry reg;
    reg.registerSystem<MockA>(SystemPhase::Update);
    EXPECT_THROW(reg.registerSystem<MockA>(SystemPhase::Update), FabricException);
}

TEST_F(SystemRegistryTest, DependencyOrdering) {
    SystemRegistry reg;
    // Register B before A, but B.after<A>() should make A init first
    reg.registerSystem<MockB>(SystemPhase::Update);
    reg.registerSystem<MockA>(SystemPhase::Update);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();
    reg.initAll(ctx);

    ASSERT_GE(callLog.size(), 2u);
    EXPECT_EQ(callLog[0], "A::init");
    EXPECT_EQ(callLog[1], "B::init");
}

TEST_F(SystemRegistryTest, BeforeConstraint) {
    SystemRegistry reg;
    // D.before<A>() means D should init before A
    reg.registerSystem<MockA>(SystemPhase::Update);
    reg.registerSystem<MockD>(SystemPhase::Update);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();
    reg.initAll(ctx);

    auto dPos = std::find(callLog.begin(), callLog.end(), "D::init");
    auto aPos = std::find(callLog.begin(), callLog.end(), "A::init");
    ASSERT_NE(dPos, callLog.end());
    ASSERT_NE(aPos, callLog.end());
    EXPECT_LT(dPos, aPos);
}

TEST_F(SystemRegistryTest, CycleDetection) {
    SystemRegistry reg;
    reg.registerSystem<CycleP>(SystemPhase::Update);
    reg.registerSystem<CycleQ>(SystemPhase::Update);
    EXPECT_FALSE(reg.resolve());
}

TEST_F(SystemRegistryTest, EmptyRegistryResolves) {
    SystemRegistry reg;
    EXPECT_TRUE(reg.resolve());
}

TEST_F(SystemRegistryTest, PhaseDispatchCallsCorrectMethod) {
    SystemRegistry reg;
    auto& a = reg.registerSystem<MockA>(SystemPhase::FixedUpdate);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();
    reg.initAll(ctx);

    // runUpdate should NOT call a FixedUpdate system
    reg.runUpdate(ctx, 0.016f);
    EXPECT_EQ(a.updateCount, 0);

    // runFixedUpdate should call the system
    reg.runFixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_EQ(a.fixedUpdateCount, 1);
}

TEST_F(SystemRegistryTest, EnableDisable) {
    SystemRegistry reg;
    auto& a = reg.registerSystem<MockA>(SystemPhase::Update);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();
    EXPECT_TRUE(reg.isEnabled<MockA>());

    reg.setEnabled<MockA>(false);
    EXPECT_FALSE(reg.isEnabled<MockA>());

    reg.runUpdate(ctx, 0.016f);
    EXPECT_EQ(a.updateCount, 0);

    reg.setEnabled<MockA>(true);
    reg.runUpdate(ctx, 0.016f);
    EXPECT_EQ(a.updateCount, 1);
}

TEST_F(SystemRegistryTest, ShutdownReverseOrder) {
    SystemRegistry reg;
    reg.registerSystem<MockA>(SystemPhase::Update);
    reg.registerSystem<MockB>(SystemPhase::Update);
    reg.registerSystem<MockC>(SystemPhase::Update);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();
    reg.initAll(ctx);
    callLog.clear();

    reg.shutdownAll();

    // Init order: A, B, C (due to dependencies)
    // Shutdown order: reverse = C, B, A
    ASSERT_GE(callLog.size(), 3u);
    EXPECT_EQ(callLog[0], "C::shutdown");
    EXPECT_EQ(callLog[1], "B::shutdown");
    EXPECT_EQ(callLog[2], "A::shutdown");
}

TEST_F(SystemRegistryTest, CrossPhaseDependency) {
    SystemRegistry reg;
    // B is in FixedUpdate but depends on A in PreUpdate
    reg.registerSystem<MockB>(SystemPhase::FixedUpdate);
    reg.registerSystem<MockA>(SystemPhase::PreUpdate);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();
    reg.initAll(ctx);

    // A should init before B regardless of phase
    ASSERT_GE(callLog.size(), 2u);
    EXPECT_EQ(callLog[0], "A::init");
    EXPECT_EQ(callLog[1], "B::init");
}

TEST_F(SystemRegistryTest, ListSystemsReturnsAll) {
    SystemRegistry reg;
    reg.registerSystem<MockA>(SystemPhase::Update);
    reg.registerSystem<MockB>(SystemPhase::FixedUpdate);
    ASSERT_TRUE(reg.resolve());

    auto systems = reg.listSystems();
    EXPECT_EQ(systems.size(), 2u);

    // Find MockA in the list
    bool foundA = false;
    for (const auto& info : systems) {
        if (info.typeId == std::type_index(typeid(MockA))) {
            foundA = true;
            EXPECT_EQ(info.phase, SystemPhase::Update);
            EXPECT_TRUE(info.enabled);
        }
    }
    EXPECT_TRUE(foundA);
}

TEST_F(SystemRegistryTest, GetUnregisteredReturnsNull) {
    SystemRegistry reg;
    EXPECT_EQ(reg.get<MockA>(), nullptr);
}

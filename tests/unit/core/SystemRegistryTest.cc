#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ConfigManager.hh"
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

    AssetRegistry assetRegistry{hub};
    SystemRegistry sysReg_;
    ConfigManager configManager;

    AppContext makeContext() {
        return AppContext{
            .world = world,
            .timeline = timeline,
            .dispatcher = dispatcher,
            .resourceHub = hub,
            .assetRegistry = assetRegistry,
            .systemRegistry = sysReg_,
            .configManager = configManager,
        };
    }

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

// ── Cycle detection: 3-node cycle A->B->C->A ──────────────────────────

namespace {

class Cycle3A : public System<Cycle3A> {
  public:
    void configureDependencies() override;
};

class Cycle3B : public System<Cycle3B> {
  public:
    void configureDependencies() override { after<Cycle3A>(); }
};

class Cycle3C : public System<Cycle3C> {
  public:
    void configureDependencies() override { after<Cycle3B>(); }
};

// Closes the cycle: A depends on C
void Cycle3A::configureDependencies() {
    after<Cycle3C>();
}

} // namespace

TEST_F(SystemRegistryTest, ThreeNodeCycleDetection) {
    SystemRegistry reg;
    reg.registerSystem<Cycle3A>(SystemPhase::Update);
    reg.registerSystem<Cycle3B>(SystemPhase::Update);
    reg.registerSystem<Cycle3C>(SystemPhase::Update);
    EXPECT_FALSE(reg.resolve());
}

// ── Self-dependency cycle ──────────────────────────────────────────────

TEST_F(SystemRegistryTest, SelfDependencyCycle) {
    SystemRegistry reg;
    // CycleX declares after<CycleX>() (self-dependency)
    reg.registerSystem<CycleX>(SystemPhase::Update);
    EXPECT_FALSE(reg.resolve());
}

// ── Diamond dependency (A->B, A->C, B->D, C->D) ───────────────────────

namespace {

class DiamondD : public System<DiamondD> {
  public:
    void init(AppContext& ctx) override {
        if (g_callLog)
            g_callLog->push_back("DD::init");
    }
    void shutdown() override {
        if (g_callLog)
            g_callLog->push_back("DD::shutdown");
    }
};

class DiamondB : public System<DiamondB> {
  public:
    void configureDependencies() override { after<DiamondD>(); }
    void init(AppContext& ctx) override {
        if (g_callLog)
            g_callLog->push_back("DB::init");
    }
};

class DiamondC : public System<DiamondC> {
  public:
    void configureDependencies() override { after<DiamondD>(); }
    void init(AppContext& ctx) override {
        if (g_callLog)
            g_callLog->push_back("DC::init");
    }
};

class DiamondA : public System<DiamondA> {
  public:
    void configureDependencies() override {
        after<DiamondB>();
        after<DiamondC>();
    }
    void init(AppContext& ctx) override {
        if (g_callLog)
            g_callLog->push_back("DA::init");
    }
};

} // namespace

TEST_F(SystemRegistryTest, DiamondDependencyResolves) {
    SystemRegistry reg;
    reg.registerSystem<DiamondA>(SystemPhase::Update);
    reg.registerSystem<DiamondB>(SystemPhase::Update);
    reg.registerSystem<DiamondC>(SystemPhase::Update);
    reg.registerSystem<DiamondD>(SystemPhase::Update);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();
    reg.initAll(ctx);

    // D must init first (leaf), A must init last (root)
    ASSERT_GE(callLog.size(), 4u);
    EXPECT_EQ(callLog.front(), "DD::init");
    EXPECT_EQ(callLog.back(), "DA::init");
}

// ── Per-phase topological ordering within same phase ───────────────────

TEST_F(SystemRegistryTest, TopologicalOrderWithinPhase) {
    SystemRegistry reg;
    // A, B, C all in Update phase with chain C->B->A
    reg.registerSystem<MockC>(SystemPhase::Update);
    reg.registerSystem<MockA>(SystemPhase::Update);
    reg.registerSystem<MockB>(SystemPhase::Update);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();
    reg.initAll(ctx);
    callLog.clear();

    // Dispatch should respect topological order within phase
    reg.runUpdate(ctx, 0.016f);
    ASSERT_EQ(callLog.size(), 3u);
    EXPECT_EQ(callLog[0], "A::update");
    EXPECT_EQ(callLog[1], "B::update");
    EXPECT_EQ(callLog[2], "C::update");
}

// ── Dispatch on empty registry ─────────────────────────────────────────

TEST_F(SystemRegistryTest, DispatchOnEmptyRegistry) {
    SystemRegistry reg;
    ASSERT_TRUE(reg.resolve());
    auto ctx = makeContext();
    // All dispatch methods should be safe on empty registry
    reg.runPreUpdate(ctx, 0.016f);
    reg.runFixedUpdate(ctx, 1.0f / 60.0f);
    reg.runUpdate(ctx, 0.016f);
    reg.runPostUpdate(ctx, 0.016f);
    reg.runPreRender(ctx);
    reg.runRender(ctx);
    reg.runPostRender(ctx);
}

// ── Shutdown on empty registry ─────────────────────────────────────────

TEST_F(SystemRegistryTest, ShutdownOnEmptyRegistry) {
    SystemRegistry reg;
    ASSERT_TRUE(reg.resolve());
    reg.shutdownAll(); // Should not crash
}

// ── Enable/disable during dispatch ─────────────────────────────────────

TEST_F(SystemRegistryTest, DisableSkipsSystemDuringDispatch) {
    SystemRegistry reg;
    auto& a = reg.registerSystem<MockA>(SystemPhase::Update);
    auto& b = reg.registerSystem<MockB>(SystemPhase::Update);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();

    // Disable B, only A should run
    reg.setEnabled<MockB>(false);
    reg.runUpdate(ctx, 0.016f);
    EXPECT_EQ(a.updateCount, 1);
    EXPECT_EQ(b.updateCount, 0);

    // Re-enable B, both should run
    reg.setEnabled<MockB>(true);
    reg.runUpdate(ctx, 0.016f);
    EXPECT_EQ(a.updateCount, 2);
    EXPECT_EQ(b.updateCount, 1);
}

// ── Render phase dispatch verification ─────────────────────────────────

TEST_F(SystemRegistryTest, RenderPhaseDispatch) {
    SystemRegistry reg;
    auto& a = reg.registerSystem<MockA>(SystemPhase::Render);
    ASSERT_TRUE(reg.resolve());

    auto ctx = makeContext();

    // runRender should call render(), not update()
    reg.runRender(ctx);
    EXPECT_EQ(a.renderCount, 1);
    EXPECT_EQ(a.updateCount, 0);
}

// ── isEnabled returns false for unregistered system ────────────────────

TEST_F(SystemRegistryTest, IsEnabledFalseForUnregistered) {
    SystemRegistry reg;
    EXPECT_FALSE(reg.isEnabled<MockA>());
}

// ── Register after resolve should throw ────────────────────────────────

TEST_F(SystemRegistryTest, RegisterAfterResolve) {
    SystemRegistry reg;
    reg.registerSystem<MockA>(SystemPhase::Update);
    ASSERT_TRUE(reg.resolve());

    // Registration after resolve() must be rejected
    EXPECT_THROW(reg.registerSystem<MockB>(SystemPhase::Update), FabricException);
}

#include "fabric/core/SystemBase.hh"
#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemPhase.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include <gtest/gtest.h>

using namespace fabric;

namespace {

class AlphaSystem : public System<AlphaSystem> {
  public:
    void update(AppContext& ctx, float dt) override {
        (void)ctx;
        (void)dt;
    }
};

class BetaSystem : public System<BetaSystem> {
  public:
    void configureDependencies() override { after<AlphaSystem>(); }
};

class GammaSystem : public System<GammaSystem> {};

} // namespace

TEST(SystemBaseTest, NameReturnsNonEmpty) {
    AlphaSystem sys;
    std::string n = sys.name();
    EXPECT_FALSE(n.empty());
    // Demangled name should contain "Alpha" on GCC/Clang
    EXPECT_NE(n.find("Alpha"), std::string::npos);
}

TEST(SystemBaseTest, DefaultMethodsAreNoOp) {
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

    GammaSystem sys;
    sys.init(ctx);
    sys.update(ctx, 0.016f);
    sys.fixedUpdate(ctx, 1.0f / 60.0f);
    sys.render(ctx);
    sys.shutdown();
    sys.configureDependencies();
}

TEST(SystemBaseTest, TypeIdUniqueness) {
    AlphaSystem alpha;
    BetaSystem beta;
    GammaSystem gamma;

    EXPECT_NE(alpha.typeId(), beta.typeId());
    EXPECT_NE(alpha.typeId(), gamma.typeId());
    EXPECT_NE(beta.typeId(), gamma.typeId());
    EXPECT_EQ(alpha.typeId(), std::type_index(typeid(AlphaSystem)));
}

TEST(SystemBaseTest, ConfigureDependenciesDoesNotCrash) {
    BetaSystem beta;
    // Calls after<AlphaSystem>() internally, should not throw
    beta.configureDependencies();
}

// ── SystemPhase coverage ──────────────────────────────────────────────

TEST(SystemPhaseTest, ToStringAllValues) {
    EXPECT_EQ(systemPhaseToString(SystemPhase::PreUpdate), "PreUpdate");
    EXPECT_EQ(systemPhaseToString(SystemPhase::FixedUpdate), "FixedUpdate");
    EXPECT_EQ(systemPhaseToString(SystemPhase::Update), "Update");
    EXPECT_EQ(systemPhaseToString(SystemPhase::PostUpdate), "PostUpdate");
    EXPECT_EQ(systemPhaseToString(SystemPhase::PreRender), "PreRender");
    EXPECT_EQ(systemPhaseToString(SystemPhase::Render), "Render");
    EXPECT_EQ(systemPhaseToString(SystemPhase::PostRender), "PostRender");
}

TEST(SystemPhaseTest, PhaseCountMatchesEnum) {
    EXPECT_EQ(K_SYSTEM_PHASE_COUNT, 7u);
    // Verify the last enum value is at index 6
    EXPECT_EQ(static_cast<std::size_t>(SystemPhase::PostRender), 6u);
}

// ── SystemBase CRTP additional coverage ───────────────────────────────

TEST(SystemBaseTest, DuplicateDependencyDeclaration) {
    // Calling after<T>() twice should accumulate (no dedup in SystemBase)
    BetaSystem beta;
    beta.configureDependencies();
    beta.configureDependencies();
    // Should not crash; registry handles dedup during resolve
}

TEST(SystemBaseTest, NameContainsTypeName) {
    BetaSystem beta;
    GammaSystem gamma;
    std::string betaName = beta.name();
    std::string gammaName = gamma.name();
    EXPECT_NE(betaName.find("Beta"), std::string::npos);
    EXPECT_NE(gammaName.find("Gamma"), std::string::npos);
    EXPECT_NE(betaName, gammaName);
}

TEST(SystemBaseTest, PhaseAccessorOnRegisteredSystem) {
    // Phase is stored on SystemRegistry, not SystemBase. Verify via registry.
    SystemRegistry reg;
    reg.registerSystem<AlphaSystem>(SystemPhase::Render);
    ASSERT_TRUE(reg.resolve());

    auto systems = reg.listSystems();
    ASSERT_EQ(systems.size(), 1u);
    EXPECT_EQ(systems[0].phase, SystemPhase::Render);
}

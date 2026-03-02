#include "fabric/core/SystemBase.hh"
#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemRegistry.hh"
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

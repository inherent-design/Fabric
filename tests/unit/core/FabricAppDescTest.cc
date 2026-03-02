#include <gtest/gtest.h>

#include "fabric/core/FabricAppDesc.hh"

namespace fabric {

// Test systems
class TestSystemA : public System<TestSystemA> {
  public:
    bool initialized = false;
    void init(AppContext& /*ctx*/) override { initialized = true; }
};

class TestSystemB : public System<TestSystemB> {
  public:
    int value;
    explicit TestSystemB(int v) : value(v) {}
};

TEST(FabricAppDescTest, DefaultValues) {
    FabricAppDesc desc;
    EXPECT_EQ(desc.name, "Fabric");
    EXPECT_TRUE(desc.configPath.empty());
    EXPECT_EQ(desc.windowDesc.width, 1280);
    EXPECT_EQ(desc.windowDesc.height, 720);
    EXPECT_TRUE(desc.systemRegistrations_.empty());
}

TEST(FabricAppDescTest, RegisterSystemAccumulatesFactories) {
    FabricAppDesc desc;
    desc.registerSystem<TestSystemA>(SystemPhase::Update);
    desc.registerSystem<TestSystemB>(SystemPhase::FixedUpdate, 42);

    EXPECT_EQ(desc.systemRegistrations_.size(), 2u);
    EXPECT_EQ(desc.systemRegistrations_[0].phase, SystemPhase::Update);
    EXPECT_EQ(desc.systemRegistrations_[1].phase, SystemPhase::FixedUpdate);
}

TEST(FabricAppDescTest, FactoryCreatesCorrectSystem) {
    FabricAppDesc desc;
    desc.registerSystem<TestSystemB>(SystemPhase::Update, 99);

    auto system = desc.systemRegistrations_[0].factory();
    ASSERT_NE(system, nullptr);

    auto* concrete = dynamic_cast<TestSystemB*>(system.get());
    ASSERT_NE(concrete, nullptr);
    EXPECT_EQ(concrete->value, 99);
}

TEST(FabricAppDescTest, WindowDescOverrides) {
    FabricAppDesc desc;
    desc.windowDesc.title = "Custom";
    desc.windowDesc.width = 1920;
    desc.windowDesc.height = 1080;
    desc.windowDesc.fullscreen = true;

    EXPECT_EQ(desc.windowDesc.title, "Custom");
    EXPECT_EQ(desc.windowDesc.width, 1920);
    EXPECT_TRUE(desc.windowDesc.fullscreen);
}

TEST(FabricAppDescTest, LifecycleCallbacksDefaultNull) {
    FabricAppDesc desc;
    EXPECT_FALSE(static_cast<bool>(desc.onInit));
    EXPECT_FALSE(static_cast<bool>(desc.onShutdown));
    EXPECT_FALSE(static_cast<bool>(desc.onFocusGained));
    EXPECT_FALSE(static_cast<bool>(desc.onFocusLost));
    EXPECT_FALSE(static_cast<bool>(desc.onResize));
}

TEST(FabricAppDescTest, SetCallbacks) {
    FabricAppDesc desc;
    bool initCalled = false;
    bool shutdownCalled = false;

    desc.onInit = [&](AppContext& /*ctx*/) {
        initCalled = true;
    };
    desc.onShutdown = [&](AppContext& /*ctx*/) {
        shutdownCalled = true;
    };

    EXPECT_TRUE(static_cast<bool>(desc.onInit));
    EXPECT_TRUE(static_cast<bool>(desc.onShutdown));
}

} // namespace fabric

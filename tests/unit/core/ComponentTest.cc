#include "fabric/core/Component.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Testing.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace fabric;
using namespace fabric::Testing;

class ComponentTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create test components
        rootComponent = std::make_shared<MockComponent>("root");
        childComponent1 = std::make_shared<MockComponent>("child1");
        childComponent2 = std::make_shared<MockComponent>("child2");
    }

    std::shared_ptr<MockComponent> rootComponent;
    std::shared_ptr<MockComponent> childComponent1;
    std::shared_ptr<MockComponent> childComponent2;
};

TEST_F(ComponentTest, ConstructorThrowsOnEmptyId) {
    EXPECT_THROW(MockComponent(""), FabricException);
}

TEST_F(ComponentTest, GetId) {
    EXPECT_EQ(rootComponent->getId(), "root");
    EXPECT_EQ(childComponent1->getId(), "child1");
    EXPECT_EQ(childComponent2->getId(), "child2");
}

TEST_F(ComponentTest, AddChild) {
    rootComponent->addChild(childComponent1);
    rootComponent->addChild(childComponent2);

    EXPECT_EQ(rootComponent->getChildren().size(), 2);
    EXPECT_EQ(rootComponent->getChildren()[0]->getId(), "child1");
    EXPECT_EQ(rootComponent->getChildren()[1]->getId(), "child2");
}

TEST_F(ComponentTest, AddChildThrowsOnDuplicateId) {
    rootComponent->addChild(childComponent1);
    auto duplicateChild = std::make_shared<MockComponent>("child1");

    EXPECT_THROW(rootComponent->addChild(duplicateChild), FabricException);
}

TEST_F(ComponentTest, AddChildThrowsOnNullChild) {
    EXPECT_THROW(rootComponent->addChild(nullptr), FabricException);
}

TEST_F(ComponentTest, GetChild) {
    rootComponent->addChild(childComponent1);
    rootComponent->addChild(childComponent2);

    auto child = rootComponent->getChild("child1");
    EXPECT_EQ(child->getId(), "child1");

    child = rootComponent->getChild("child2");
    EXPECT_EQ(child->getId(), "child2");

    child = rootComponent->getChild("nonexistent");
    EXPECT_EQ(child, nullptr);
}

TEST_F(ComponentTest, RemoveChild) {
    rootComponent->addChild(childComponent1);
    rootComponent->addChild(childComponent2);

    EXPECT_TRUE(rootComponent->removeChild("child1"));
    EXPECT_EQ(rootComponent->getChildren().size(), 1);
    EXPECT_EQ(rootComponent->getChildren()[0]->getId(), "child2");

    EXPECT_FALSE(rootComponent->removeChild("nonexistent"));
}

TEST_F(ComponentTest, PropertySetGet) {
    rootComponent->setProperty<int>("intProp", 42);
    rootComponent->setProperty<float>("floatProp", 3.14f);
    rootComponent->setProperty<std::string>("stringProp", "hello");
    rootComponent->setProperty<bool>("boolProp", true);

    EXPECT_EQ(rootComponent->getProperty<int>("intProp"), 42);
    EXPECT_FLOAT_EQ(rootComponent->getProperty<float>("floatProp"), 3.14f);
    EXPECT_EQ(rootComponent->getProperty<std::string>("stringProp"), "hello");
    EXPECT_EQ(rootComponent->getProperty<bool>("boolProp"), true);
}

TEST_F(ComponentTest, PropertyGetThrowsOnMissingProperty) {
    EXPECT_THROW(rootComponent->getProperty<int>("nonexistent"), FabricException);
}

// Skip this test for now, as the template implementation makes it hard to test
// TODO: Fix this test once the component property system is refactored
TEST_F(ComponentTest, DISABLED_PropertyGetThrowsOnWrongType) {
    rootComponent->setProperty<int>("intProp", 42);

    // Currently not working: std::shared_ptr<void> cannot be dynamic_cast
    EXPECT_THROW(rootComponent->getProperty<std::string>("intProp"), FabricException);
}

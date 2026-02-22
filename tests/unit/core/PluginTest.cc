#include "fabric/core/Plugin.hh"
#include "fabric/utils/Testing.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace fabric;
using namespace fabric::Testing;

// Mock plugin implementation for testing
class MockPlugin : public Plugin {
public:
  std::string getName() const override { return "MockPlugin"; }
  std::string getVersion() const override { return "1.0.0"; }
  std::string getAuthor() const override { return "Test Author"; }
  std::string getDescription() const override { return "A mock plugin for testing"; }

  bool initialize() override {
    initializeCalled = true;
    return initializeResult;
  }

  void shutdown() override {
    shutdownCalled = true;
  }

  std::vector<std::shared_ptr<Component>> getComponents() override {
    std::vector<std::shared_ptr<Component>> components;
    components.push_back(std::make_shared<MockComponent>("component1"));
    components.push_back(std::make_shared<MockComponent>("component2"));
    return components;
  }

  // Test control flags
  bool initializeCalled = false;
  bool initializeResult = true;
  bool shutdownCalled = false;
};

class PluginTest : public ::testing::Test {
protected:
  void SetUp() override {
    manager.registerPlugin("MockPlugin", []() {
      return std::make_shared<MockPlugin>();
    });
  }

  PluginManager manager;
};

TEST_F(PluginTest, RegisterPlugin) {
  EXPECT_TRUE(manager.loadPlugin("MockPlugin"));
}

TEST_F(PluginTest, RegisterPluginThrowsOnEmptyName) {
  EXPECT_THROW(manager.registerPlugin("", []() {
    return std::make_shared<MockPlugin>();
  }), FabricException);
}

TEST_F(PluginTest, RegisterPluginThrowsOnNullFactory) {
  EXPECT_THROW(manager.registerPlugin("NullPlugin", nullptr), FabricException);
}

TEST_F(PluginTest, RegisterPluginThrowsOnDuplicateName) {
  EXPECT_THROW(manager.registerPlugin("MockPlugin", []() {
    return std::make_shared<MockPlugin>();
  }), FabricException);
}

TEST_F(PluginTest, LoadPlugin) {
  EXPECT_TRUE(manager.loadPlugin("MockPlugin"));

  auto plugin = manager.getPlugin("MockPlugin");
  EXPECT_NE(plugin, nullptr);
  EXPECT_EQ(plugin->getName(), "MockPlugin");
}

TEST_F(PluginTest, LoadAlreadyLoadedPlugin) {
  EXPECT_TRUE(manager.loadPlugin("MockPlugin"));
  EXPECT_TRUE(manager.loadPlugin("MockPlugin")); // Should return true for already loaded
}

TEST_F(PluginTest, LoadNonexistentPlugin) {
  EXPECT_FALSE(manager.loadPlugin("NonexistentPlugin"));
}

TEST_F(PluginTest, GetPlugin) {
  manager.loadPlugin("MockPlugin");

  auto plugin = manager.getPlugin("MockPlugin");
  EXPECT_NE(plugin, nullptr);
  EXPECT_EQ(plugin->getName(), "MockPlugin");
  EXPECT_EQ(plugin->getVersion(), "1.0.0");
  EXPECT_EQ(plugin->getAuthor(), "Test Author");
  EXPECT_EQ(plugin->getDescription(), "A mock plugin for testing");
}

TEST_F(PluginTest, GetNonexistentPlugin) {
  EXPECT_EQ(manager.getPlugin("NonexistentPlugin"), nullptr);
}

TEST_F(PluginTest, GetPlugins) {
  manager.loadPlugin("MockPlugin");
  const auto& plugins = manager.getPlugins();

  EXPECT_EQ(plugins.size(), 1);
  EXPECT_TRUE(plugins.find("MockPlugin") != plugins.end());
}

TEST_F(PluginTest, UnloadPlugin) {
  manager.loadPlugin("MockPlugin");
  EXPECT_TRUE(manager.unloadPlugin("MockPlugin"));

  // Should now be unloaded
  EXPECT_EQ(manager.getPlugin("MockPlugin"), nullptr);
  EXPECT_EQ(manager.getPlugins().size(), 0);
}

TEST_F(PluginTest, UnloadNonexistentPlugin) {
  EXPECT_FALSE(manager.unloadPlugin("NonexistentPlugin"));
}

TEST_F(PluginTest, InitializeAll) {
  manager.loadPlugin("MockPlugin");
  EXPECT_TRUE(manager.initializeAll());

  auto pluginObj = std::dynamic_pointer_cast<MockPlugin>(
    manager.getPlugin("MockPlugin")
  );
  EXPECT_TRUE(pluginObj->initializeCalled);
}

TEST_F(PluginTest, InitializeAllFailure) {
  manager.registerPlugin("FailingPlugin", []() {
    auto plugin = std::make_shared<MockPlugin>();
    plugin->initializeResult = false;
    return plugin;
  });

  manager.loadPlugin("FailingPlugin");
  EXPECT_FALSE(manager.initializeAll());
}

TEST_F(PluginTest, ShutdownAll) {
  manager.loadPlugin("MockPlugin");
  auto pluginObj = std::dynamic_pointer_cast<MockPlugin>(
    manager.getPlugin("MockPlugin")
  );

  manager.shutdownAll();
  EXPECT_TRUE(pluginObj->shutdownCalled);
  EXPECT_EQ(manager.getPlugins().size(), 0);
}

TEST_F(PluginTest, GetComponents) {
  manager.loadPlugin("MockPlugin");
  auto plugin = manager.getPlugin("MockPlugin");

  auto components = plugin->getComponents();
  EXPECT_EQ(components.size(), 2);
  EXPECT_EQ(components[0]->getId(), "component1");
  EXPECT_EQ(components[1]->getId(), "component2");
}

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
    // Reset the plugin manager instance for each test
    PluginManager::getInstance().shutdownAll();
    // Register the test plugin
    try {
      PluginManager::getInstance().registerPlugin("MockPlugin", []() {
        return std::make_shared<MockPlugin>();
      });
    } catch (const FabricException&) {
      // Plugin might already be registered from a previous test
      // We'll handle this in specific tests
    }
  }
  
  void TearDown() override {
    // Clean up loaded plugins
    PluginManager::getInstance().shutdownAll();
  }
};

TEST_F(PluginTest, RegisterPlugin) {
  // Plugin was registered in SetUp
  EXPECT_TRUE(PluginManager::getInstance().loadPlugin("MockPlugin"));
}

TEST_F(PluginTest, RegisterPluginThrowsOnEmptyName) {
  EXPECT_THROW(PluginManager::getInstance().registerPlugin("", []() {
    return std::make_shared<MockPlugin>();
  }), FabricException);
}

TEST_F(PluginTest, RegisterPluginThrowsOnNullFactory) {
  EXPECT_THROW(PluginManager::getInstance().registerPlugin("NullPlugin", nullptr), FabricException);
}

TEST_F(PluginTest, RegisterPluginThrowsOnDuplicateName) {
  EXPECT_THROW(PluginManager::getInstance().registerPlugin("MockPlugin", []() {
    return std::make_shared<MockPlugin>();
  }), FabricException);
}

TEST_F(PluginTest, LoadPlugin) {
  EXPECT_TRUE(PluginManager::getInstance().loadPlugin("MockPlugin"));
  
  auto plugin = PluginManager::getInstance().getPlugin("MockPlugin");
  EXPECT_NE(plugin, nullptr);
  EXPECT_EQ(plugin->getName(), "MockPlugin");
}

TEST_F(PluginTest, LoadAlreadyLoadedPlugin) {
  EXPECT_TRUE(PluginManager::getInstance().loadPlugin("MockPlugin"));
  EXPECT_TRUE(PluginManager::getInstance().loadPlugin("MockPlugin")); // Should return true for already loaded
}

TEST_F(PluginTest, LoadNonexistentPlugin) {
  EXPECT_FALSE(PluginManager::getInstance().loadPlugin("NonexistentPlugin"));
}

TEST_F(PluginTest, GetPlugin) {
  PluginManager::getInstance().loadPlugin("MockPlugin");
  
  auto plugin = PluginManager::getInstance().getPlugin("MockPlugin");
  EXPECT_NE(plugin, nullptr);
  EXPECT_EQ(plugin->getName(), "MockPlugin");
  EXPECT_EQ(plugin->getVersion(), "1.0.0");
  EXPECT_EQ(plugin->getAuthor(), "Test Author");
  EXPECT_EQ(plugin->getDescription(), "A mock plugin for testing");
}

TEST_F(PluginTest, GetNonexistentPlugin) {
  EXPECT_EQ(PluginManager::getInstance().getPlugin("NonexistentPlugin"), nullptr);
}

TEST_F(PluginTest, GetPlugins) {
  PluginManager::getInstance().loadPlugin("MockPlugin");
  const auto& plugins = PluginManager::getInstance().getPlugins();
  
  EXPECT_EQ(plugins.size(), 1);
  EXPECT_TRUE(plugins.find("MockPlugin") != plugins.end());
}

TEST_F(PluginTest, UnloadPlugin) {
  PluginManager::getInstance().loadPlugin("MockPlugin");
  EXPECT_TRUE(PluginManager::getInstance().unloadPlugin("MockPlugin"));
  
  // Should now be unloaded
  EXPECT_EQ(PluginManager::getInstance().getPlugin("MockPlugin"), nullptr);
  EXPECT_EQ(PluginManager::getInstance().getPlugins().size(), 0);
}

TEST_F(PluginTest, UnloadNonexistentPlugin) {
  EXPECT_FALSE(PluginManager::getInstance().unloadPlugin("NonexistentPlugin"));
}

TEST_F(PluginTest, InitializeAll) {
  PluginManager::getInstance().loadPlugin("MockPlugin");
  EXPECT_TRUE(PluginManager::getInstance().initializeAll());
  
  auto pluginObj = std::dynamic_pointer_cast<MockPlugin>(
    PluginManager::getInstance().getPlugin("MockPlugin")
  );
  EXPECT_TRUE(pluginObj->initializeCalled);
}

TEST_F(PluginTest, InitializeAllFailure) {
  // Register a plugin that fails to initialize
  PluginManager::getInstance().registerPlugin("FailingPlugin", []() {
    auto plugin = std::make_shared<MockPlugin>();
    plugin->initializeResult = false;
    return plugin;
  });
  
  PluginManager::getInstance().loadPlugin("FailingPlugin");
  EXPECT_FALSE(PluginManager::getInstance().initializeAll());
}

TEST_F(PluginTest, ShutdownAll) {
  PluginManager::getInstance().loadPlugin("MockPlugin");
  auto pluginObj = std::dynamic_pointer_cast<MockPlugin>(
    PluginManager::getInstance().getPlugin("MockPlugin")
  );
  
  PluginManager::getInstance().shutdownAll();
  EXPECT_TRUE(pluginObj->shutdownCalled);
  EXPECT_EQ(PluginManager::getInstance().getPlugins().size(), 0);
}

TEST_F(PluginTest, GetComponents) {
  PluginManager::getInstance().loadPlugin("MockPlugin");
  auto plugin = PluginManager::getInstance().getPlugin("MockPlugin");
  
  auto components = plugin->getComponents();
  EXPECT_EQ(components.size(), 2);
  EXPECT_EQ(components[0]->getId(), "component1");
  EXPECT_EQ(components[1]->getId(), "component2");
}


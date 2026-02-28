#include "fabric/core/Plugin.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Testing.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace fabric;
using namespace fabric::Testing;

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

    void shutdown() override { shutdownCalled = true; }

    std::vector<std::shared_ptr<Component>> getComponents() override {
        std::vector<std::shared_ptr<Component>> components;
        components.push_back(std::make_shared<MockComponent>("component1"));
        components.push_back(std::make_shared<MockComponent>("component2"));
        return components;
    }

    bool initializeCalled = false;
    bool initializeResult = true;
    bool shutdownCalled = false;
};

class PluginWithDependencies : public Plugin {
  public:
    std::string getName() const override { return name_; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getAuthor() const override { return "Test Author"; }
    std::string getDescription() const override { return "Plugin with dependencies"; }

    explicit PluginWithDependencies(const std::string& name, const std::vector<std::string>& deps)
        : name_(name), dependencies_(deps) {}

    bool initialize() override {
        initializeCalled = true;
        initializeOrder.push_back(name_);
        return true;
    }

    void shutdown() override {
        shutdownCalled = true;
        shutdownOrder.push_back(name_);
    }

    std::vector<std::shared_ptr<Component>> getComponents() override { return {}; }

    std::vector<std::string> getDependencies() const override { return dependencies_; }

    bool initializeCalled = false;
    bool shutdownCalled = false;
    static std::vector<std::string> initializeOrder;
    static std::vector<std::string> shutdownOrder;

  private:
    std::string name_;
    std::vector<std::string> dependencies_;
};

std::vector<std::string> PluginWithDependencies::initializeOrder;
std::vector<std::string> PluginWithDependencies::shutdownOrder;

class PluginTest : public ::testing::Test {
  protected:
    void SetUp() override {
        PluginWithDependencies::initializeOrder.clear();
        PluginWithDependencies::shutdownOrder.clear();
    }

    PluginManager manager;
};

TEST_F(PluginTest, RegisterPlugin) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
    EXPECT_TRUE(manager.loadPlugin("MockPlugin"));

    auto plugin = manager.getPlugin("MockPlugin");
    EXPECT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->getName(), "MockPlugin");
}

TEST_F(PluginTest, RegisterPluginThrowsOnEmptyName) {
    EXPECT_THROW(manager.registerPlugin("", []() { return std::make_shared<MockPlugin>(); }), FabricException);
}

TEST_F(PluginTest, RegisterPluginThrowsOnNullFactory) {
    EXPECT_THROW(manager.registerPlugin("NullPlugin", nullptr), FabricException);
}

TEST_F(PluginTest, RegisterPluginThrowsOnDuplicateName) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });

    EXPECT_THROW(manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); }),
                 FabricException);
}

TEST_F(PluginTest, LoadPlugin) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
    EXPECT_TRUE(manager.loadPlugin("MockPlugin"));

    auto plugin = manager.getPlugin("MockPlugin");
    EXPECT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->getName(), "MockPlugin");
}

TEST_F(PluginTest, LoadAlreadyLoadedPlugin) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
    EXPECT_TRUE(manager.loadPlugin("MockPlugin"));
    EXPECT_TRUE(manager.loadPlugin("MockPlugin"));
}

TEST_F(PluginTest, LoadNonexistentPlugin) {
    EXPECT_FALSE(manager.loadPlugin("NonexistentPlugin"));
}

TEST_F(PluginTest, GetPlugin) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
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
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
    manager.loadPlugin("MockPlugin");

    const auto& plugins = manager.getPlugins();
    EXPECT_EQ(plugins.size(), 1);
    EXPECT_TRUE(plugins.find("MockPlugin") != plugins.end());
}

TEST_F(PluginTest, UnloadPlugin) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
    manager.loadPlugin("MockPlugin");
    EXPECT_TRUE(manager.unloadPlugin("MockPlugin"));

    EXPECT_EQ(manager.getPlugin("MockPlugin"), nullptr);
    EXPECT_EQ(manager.getPlugins().size(), 0);
}

TEST_F(PluginTest, UnloadNonexistentPlugin) {
    EXPECT_FALSE(manager.unloadPlugin("NonexistentPlugin"));
}

TEST_F(PluginTest, InitializeAll) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
    manager.loadPlugin("MockPlugin");
    EXPECT_TRUE(manager.initializeAll());

    auto pluginObj = std::dynamic_pointer_cast<MockPlugin>(manager.getPlugin("MockPlugin"));
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
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
    manager.loadPlugin("MockPlugin");

    auto pluginObj = std::dynamic_pointer_cast<MockPlugin>(manager.getPlugin("MockPlugin"));

    manager.shutdownAll();
    EXPECT_TRUE(pluginObj->shutdownCalled);
    EXPECT_EQ(manager.getPlugins().size(), 0);
}

TEST_F(PluginTest, GetComponents) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });
    manager.loadPlugin("MockPlugin");

    auto plugin = manager.getPlugin("MockPlugin");

    auto components = plugin->getComponents();
    EXPECT_EQ(components.size(), 2);
    EXPECT_EQ(components[0]->getId(), "component1");
    EXPECT_EQ(components[1]->getId(), "component2");
}

TEST_F(PluginTest, SimpleDependencyChain) {
    manager.registerPlugin(
        "PluginA", []() { return std::make_shared<PluginWithDependencies>("PluginA", std::vector<std::string>{}); });
    manager.registerPlugin("PluginB", []() {
        return std::make_shared<PluginWithDependencies>("PluginB", std::vector<std::string>{"PluginA"});
    });
    manager.registerPlugin("PluginC", []() {
        return std::make_shared<PluginWithDependencies>("PluginC", std::vector<std::string>{"PluginB"});
    });

    manager.loadPlugin("PluginA");
    manager.loadPlugin("PluginB");
    manager.loadPlugin("PluginC");

    EXPECT_TRUE(manager.initializeAll());

    const auto& order = PluginWithDependencies::initializeOrder;
    EXPECT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], "PluginA");
    EXPECT_EQ(order[1], "PluginB");
    EXPECT_EQ(order[2], "PluginC");
}

TEST_F(PluginTest, ShutdownReverseDependencyOrder) {
    manager.registerPlugin(
        "PluginA", []() { return std::make_shared<PluginWithDependencies>("PluginA", std::vector<std::string>{}); });
    manager.registerPlugin("PluginB", []() {
        return std::make_shared<PluginWithDependencies>("PluginB", std::vector<std::string>{"PluginA"});
    });
    manager.registerPlugin("PluginC", []() {
        return std::make_shared<PluginWithDependencies>("PluginC", std::vector<std::string>{"PluginB"});
    });

    manager.loadPlugin("PluginA");
    manager.loadPlugin("PluginB");
    manager.loadPlugin("PluginC");

    manager.shutdownAll();

    const auto& shutdownOrder = PluginWithDependencies::shutdownOrder;
    EXPECT_EQ(shutdownOrder.size(), 3);
    EXPECT_EQ(shutdownOrder[0], "PluginC");
    EXPECT_EQ(shutdownOrder[1], "PluginB");
    EXPECT_EQ(shutdownOrder[2], "PluginA");
}

TEST_F(PluginTest, DependencyCycleDetection) {
    manager.registerPlugin("PluginA", []() {
        return std::make_shared<PluginWithDependencies>("PluginA", std::vector<std::string>{"PluginB"});
    });
    manager.registerPlugin("PluginB", []() {
        return std::make_shared<PluginWithDependencies>("PluginB", std::vector<std::string>{"PluginC"});
    });
    manager.registerPlugin("PluginC", []() {
        return std::make_shared<PluginWithDependencies>("PluginC", std::vector<std::string>{"PluginA"});
    });

    EXPECT_FALSE(manager.loadPlugin("PluginA"));
}

TEST_F(PluginTest, GetInitializationOrder) {
    manager.registerPlugin(
        "PluginA", []() { return std::make_shared<PluginWithDependencies>("PluginA", std::vector<std::string>{}); });
    manager.registerPlugin("PluginB", []() {
        return std::make_shared<PluginWithDependencies>("PluginB", std::vector<std::string>{"PluginA"});
    });
    manager.registerPlugin("PluginC", []() {
        return std::make_shared<PluginWithDependencies>("PluginC", std::vector<std::string>{"PluginB"});
    });

    manager.loadPlugin("PluginA");
    manager.loadPlugin("PluginB");
    manager.loadPlugin("PluginC");

    auto order = manager.getInitializationOrder();
    EXPECT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], "PluginA");
    EXPECT_EQ(order[1], "PluginB");
    EXPECT_EQ(order[2], "PluginC");
}

TEST_F(PluginTest, HasDependencyCycle) {
    manager.registerPlugin("PluginA", []() {
        return std::make_shared<PluginWithDependencies>("PluginA", std::vector<std::string>{"PluginB"});
    });
    manager.registerPlugin("PluginB", []() {
        return std::make_shared<PluginWithDependencies>("PluginB", std::vector<std::string>{"PluginA"});
    });
    manager.registerPlugin("PluginC", []() {
        return std::make_shared<PluginWithDependencies>("PluginC", std::vector<std::string>{"PluginA"});
    });

    EXPECT_TRUE(manager.hasDependencyCycle());
    EXPECT_FALSE(manager.loadPlugin("PluginA"));
}

TEST_F(PluginTest, IndependentPluginsAnyOrder) {
    manager.registerPlugin(
        "PluginA", []() { return std::make_shared<PluginWithDependencies>("PluginA", std::vector<std::string>{}); });
    manager.registerPlugin(
        "PluginB", []() { return std::make_shared<PluginWithDependencies>("PluginB", std::vector<std::string>{}); });
    manager.registerPlugin(
        "PluginC", []() { return std::make_shared<PluginWithDependencies>("PluginC", std::vector<std::string>{}); });

    manager.loadPlugin("PluginA");
    manager.loadPlugin("PluginB");
    manager.loadPlugin("PluginC");

    EXPECT_FALSE(manager.hasDependencyCycle());

    auto order = manager.getInitializationOrder();
    EXPECT_EQ(order.size(), 3);
}

TEST_F(PluginTest, MissingDependencyNotRegistered) {
    manager.registerPlugin("PluginA", []() {
        return std::make_shared<PluginWithDependencies>("PluginA", std::vector<std::string>{"MissingDep"});
    });

    EXPECT_TRUE(manager.loadPlugin("PluginA"));
}

TEST_F(PluginTest, LibraryPathTracking) {
    manager.registerPlugin("MockPlugin", []() {
        auto plugin = std::make_shared<MockPlugin>();
        plugin->setLibraryPath("/path/to/plugin.so");
        return plugin;
    });
    manager.loadPlugin("MockPlugin");

    auto plugin = manager.getPlugin("MockPlugin");
    EXPECT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->getLibraryPath(), "/path/to/plugin.so");
}

TEST_F(PluginTest, ReloadAfterLibraryChange) {
    manager.registerPlugin("MockPlugin", []() {
        auto plugin = std::make_shared<MockPlugin>();
        plugin->setLibraryPath("/path/to/plugin.so");
        return plugin;
    });
    manager.loadPlugin("MockPlugin");

    auto plugin1 = std::dynamic_pointer_cast<MockPlugin>(manager.getPlugin("MockPlugin"));
    EXPECT_NE(plugin1, nullptr);

    EXPECT_TRUE(manager.unloadPlugin("MockPlugin"));

    manager.registerPlugin("MockPluginV2", []() {
        auto plugin = std::make_shared<MockPlugin>();
        plugin->setLibraryPath("/new/path/plugin.so");
        return plugin;
    });
    EXPECT_TRUE(manager.loadPlugin("MockPluginV2"));

    auto plugin2 = std::dynamic_pointer_cast<MockPlugin>(manager.getPlugin("MockPluginV2"));
    EXPECT_NE(plugin2, nullptr);
    EXPECT_EQ(plugin2->getLibraryPath(), "/new/path/plugin.so");
}

TEST_F(PluginTest, ReloadPluginRecreatesLoadedInstance) {
    manager.registerPlugin("MockPlugin", []() {
        auto plugin = std::make_shared<MockPlugin>();
        plugin->setLibraryPath("/path/to/plugin.so");
        return plugin;
    });

    EXPECT_TRUE(manager.loadPlugin("MockPlugin"));
    auto pluginBefore = manager.getPlugin("MockPlugin");
    EXPECT_NE(pluginBefore, nullptr);

    EXPECT_TRUE(manager.reloadPlugin("MockPlugin"));

    auto pluginAfter = manager.getPlugin("MockPlugin");
    EXPECT_NE(pluginAfter, nullptr);
    EXPECT_NE(pluginBefore.get(), pluginAfter.get());
}

TEST_F(PluginTest, ReloadPluginFailsWhenNotLoaded) {
    manager.registerPlugin("MockPlugin", []() { return std::make_shared<MockPlugin>(); });

    EXPECT_FALSE(manager.reloadPlugin("MockPlugin"));
}

TEST_F(PluginTest, ReloadPluginFailsWhenNotRegistered) {
    EXPECT_FALSE(manager.reloadPlugin("NonexistentPlugin"));
}

TEST_F(PluginTest, FileWatcherAccessible) {
    auto& watcher = manager.getFileWatcher();
    EXPECT_FALSE(watcher.isValid());
}

TEST_F(PluginTest, FileWatcherInitViaHotReload) {
    // enableHotReload initializes the file watcher
    manager.enableHotReload("/tmp");
    auto& watcher = manager.getFileWatcher();
    EXPECT_TRUE(watcher.isValid());
    watcher.shutdown();
}

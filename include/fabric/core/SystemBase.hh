#pragma once

#include <string>
#include <typeindex>
#include <vector>

namespace fabric {

struct AppContext;

class SystemRegistry;

class SystemBase {
  public:
    virtual ~SystemBase() = default;

    /// Human-readable name for logging and debugging.
    /// Default returns demangled type name (GCC/Clang) or raw typeid name.
    virtual std::string name() const;

    /// Non-virtual lifecycle wrappers (call these, don't override).
    /// Idempotent: init runs doInit once, shutdown runs doShutdown once.
    void init(AppContext& ctx);
    void shutdown();

    bool isInitialized() const { return initialized_; }
    bool isShutDown() const { return shutDown_; }

    /// Called each frame for PreUpdate, Update, PostUpdate phase systems.
    virtual void update(AppContext& ctx, float dt) {
        (void)ctx;
        (void)dt;
    }

    /// Called each fixed timestep for FixedUpdate phase systems.
    virtual void fixedUpdate(AppContext& ctx, float fixedDt) {
        (void)ctx;
        (void)fixedDt;
    }

    /// Called each frame for PreRender, Render, PostRender phase systems.
    virtual void render(AppContext& ctx) { (void)ctx; }

    /// Override to declare ordering constraints relative to other systems.
    /// Called during resolve(), before init().
    virtual void configureDependencies() {}

    /// Type identity for registry lookups.
    virtual std::type_index typeId() const = 0;

  protected:
    /// Subclasses override these instead of init()/shutdown().
    virtual void doInit(AppContext& ctx) { (void)ctx; }
    virtual void doShutdown() {}

    /// Declare: this system must run after the given dependency.
    void after(std::type_index dep);
    void before(std::type_index dep);

    template <typename T> void after() { after(std::type_index(typeid(T))); }

    template <typename T> void before() { before(std::type_index(typeid(T))); }

  private:
    friend class SystemRegistry;
    std::vector<std::type_index> afterDeps_;
    std::vector<std::type_index> beforeDeps_;
    bool initialized_ = false;
    bool shutDown_ = false;
};

/// CRTP helper that provides typeId() automatically.
template <typename Derived> class System : public SystemBase {
    System() = default;

  public:
    std::type_index typeId() const override { return std::type_index(typeid(Derived)); }
    friend Derived;
};

} // namespace fabric

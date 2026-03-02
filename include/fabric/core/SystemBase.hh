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

    /// Lifecycle: called once after all systems are registered and sorted.
    virtual void init(AppContext& ctx) { (void)ctx; }

    /// Lifecycle: called once in reverse init order during shutdown.
    virtual void shutdown() {}

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
    /// Declare: this system must run after the given dependency.
    void after(std::type_index dep);
    void before(std::type_index dep);

    template <typename T> void after() { after(std::type_index(typeid(T))); }

    template <typename T> void before() { before(std::type_index(typeid(T))); }

  private:
    friend class SystemRegistry;
    std::vector<std::type_index> afterDeps_;
    std::vector<std::type_index> beforeDeps_;
};

/// CRTP helper that provides typeId() automatically.
template <typename Derived> class System : public SystemBase {
  public:
    std::type_index typeId() const override { return std::type_index(typeid(Derived)); }
};

} // namespace fabric

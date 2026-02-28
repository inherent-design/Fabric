#pragma once

#include <cstdint>

namespace fabric {

/// Flags controlling which debug overlays are active.
enum class DebugDrawFlags : uint32_t {
    None = 0,
    Wireframe = 1 << 0,
    CollisionShapes = 1 << 1,
    BVHOverlay = 1 << 2,
};

constexpr DebugDrawFlags operator|(DebugDrawFlags a, DebugDrawFlags b) {
    return static_cast<DebugDrawFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr DebugDrawFlags operator&(DebugDrawFlags a, DebugDrawFlags b) {
    return static_cast<DebugDrawFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr DebugDrawFlags operator~(DebugDrawFlags a) {
    return static_cast<DebugDrawFlags>(~static_cast<uint32_t>(a));
}

inline DebugDrawFlags& operator|=(DebugDrawFlags& a, DebugDrawFlags b) {
    a = a | b;
    return a;
}

inline DebugDrawFlags& operator&=(DebugDrawFlags& a, DebugDrawFlags b) {
    a = a & b;
    return a;
}

/// Thin wrapper around bgfx debugdraw with flag-based overlay control
/// and F4 wireframe toggle.
class DebugDraw {
  public:
    DebugDraw() = default;
    ~DebugDraw();

    DebugDraw(const DebugDraw&) = delete;
    DebugDraw& operator=(const DebugDraw&) = delete;

    /// Call after bgfx::init()
    void init();

    /// Call before bgfx::shutdown()
    void shutdown();

    bool isInitialized() const;

    // Flag management
    void toggleFlag(DebugDrawFlags flag);
    void setFlag(DebugDrawFlags flag, bool enabled);
    bool hasFlag(DebugDrawFlags flag) const;
    DebugDrawFlags flags() const;

    // Wireframe convenience (wraps setFlag for Wireframe)
    void toggleWireframe();
    bool isWireframeEnabled() const;

    /// Apply current wireframe state to bgfx debug flags.
    /// Call once per frame before rendering.
    void applyDebugFlags();

    /// Begin a debug draw pass on the given view.
    void begin(uint16_t viewId);

    /// End the current debug draw pass.
    void end();

    /// Drawing primitives (call between begin/end).
    void setColor(uint32_t abgr);
    void setWireframe(bool enabled);
    void drawWireBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);

  private:
    DebugDrawFlags flags_ = DebugDrawFlags::None;
    bool initialized_ = false;
    void* encoder_ = nullptr; // opaque DebugDrawEncoder*
};

} // namespace fabric

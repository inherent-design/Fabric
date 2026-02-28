#pragma once

#include "fabric/core/Spatial.hh"

#include <array>
#include <bgfx/bgfx.h>
#include <cstddef>
#include <cstdint>

namespace fabric {

enum class ParticleType : uint8_t {
    DebrisPuff,
    AmbientDust,
    Spark
};

struct Particle {
    Vector3<float, Space::World> position;
    Vector3<float, Space::World> velocity;
    float colorR, colorG, colorB, colorA;
    float size;
    float rotation;
    float rotationSpeed;
    float lifetime;
    float age;
    float drag;
    float gravityScale;
    ParticleType type;
};

// CPU-driven particle system rendered via bgfx instancing.
// Fixed pool (swap-and-pop), transient instance buffers, billboard quads.
class ParticleSystem {
  public:
    static constexpr size_t kMaxParticles = 10000;
    static constexpr uint8_t kViewId = 10;
    static constexpr size_t kInstanceStride = 48; // 3 x vec4

    ParticleSystem();
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    // Create shader program and quad vertex/index buffers. Requires live bgfx.
    void init();
    void shutdown();

    // Emit particles at a world position within a radius.
    void emit(const Vector3<float, Space::World>& pos, float radius, int count,
              ParticleType type = ParticleType::DebrisPuff);

    // Integrate velocities, apply drag/gravity, age particles, kill expired.
    void update(float dt);

    // Build transient instance buffer and submit a single instanced draw call.
    // viewMtx/projMtx are float[16] from Camera.
    void render(const float* viewMtx, const float* projMtx, uint16_t width, uint16_t height);

    size_t activeCount() const;
    bool isValid() const;

  private:
    void killParticle(size_t index);
    void initPreset(Particle& p, const Vector3<float, Space::World>& pos, float radius, ParticleType type);

    std::array<Particle, kMaxParticles> particles_;
    size_t activeCount_ = 0;

    bgfx::ProgramHandle program_;
    bgfx::VertexBufferHandle vbh_;
    bgfx::IndexBufferHandle ibh_;
    bool initialized_ = false;
};

} // namespace fabric

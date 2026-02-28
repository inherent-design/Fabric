#pragma once

#include "fabric/core/Spatial.hh"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace fabric {

struct Debris {
    Vector3<float, Space::World> position;
    Vector3<float, Space::World> velocity;
    float density;
    float radius;
    float lifetime;
    bool sleeping;
    int sleepFrames;
};

using ParticleEmitter = std::function<void(const Vector3<float, Space::World>&, float, int)>;

class DebrisPool {
  public:
    static constexpr size_t kDefaultMaxActive = 500;

    explicit DebrisPool(size_t maxActive = kDefaultMaxActive);
    ~DebrisPool() = default;

    void add(int x, int y, int z, float density);
    void add(const Vector3<float, Space::World>& pos, float density, float radius);
    void update(float dt);
    void clear();

    size_t activeCount() const;
    size_t maxActive() const;
    void setMaxActive(size_t max);
    void setMergeDistance(float distance);
    void setSleepThreshold(float velocityThreshold);
    void setSleepFrames(int frames);
    void setParticleEmitter(ParticleEmitter emitter);
    void enableParticleConversion(bool enable);
    void setParticleConvertLifetime(float lifetime);

    const std::vector<Debris>& getDebris() const;

  private:
    struct PendingDebris {
        Vector3<float, Space::World> position;
        float density;
        float radius;
    };

    void mergeNearby();
    void updateSleeping(float dt);
    void convertToParticles();
    bool shouldMerge(const Debris& a, const Debris& b) const;
    Debris merge(const Debris& a, const Debris& b) const;

    std::vector<Debris> debris_;
    std::deque<PendingDebris> pending_;
    size_t maxActive_;
    float mergeDistance_;
    float sleepThreshold_;
    int sleepFrames_;
    ParticleEmitter particleEmitter_;
    bool particleConversionEnabled_;
    float particleConvertLifetime_;
};

} // namespace fabric

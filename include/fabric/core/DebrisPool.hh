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

    struct CellKey {
        int32_t x, y, z;
        bool operator==(const CellKey& other) const { return x == other.x && y == other.y && z == other.z; }
    };

    struct CellKeyHash {
        size_t operator()(const CellKey& k) const {
            size_t h = std::hash<int32_t>{}(k.x);
            h ^= std::hash<int32_t>{}(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int32_t>{}(k.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    static CellKey toCell(const Vector3<float, Space::World>& pos, float invCellSize);

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

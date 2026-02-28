#include "fabric/core/DebrisPool.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace fabric {

DebrisPool::DebrisPool(size_t maxActive)
    : maxActive_(maxActive),
      mergeDistance_(0.5f),
      sleepThreshold_(0.01f),
      sleepFrames_(60),
      particleConversionEnabled_(false),
      particleConvertLifetime_(10.0f) {}

void DebrisPool::add(int x, int y, int z, float density) {
    add(Vector3<float, Space::World>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)), density,
        0.25f);
}

void DebrisPool::add(const Vector3<float, Space::World>& pos, float density, float radius) {
    pending_.push_back({pos, density, radius});
}

void DebrisPool::update(float dt) {
    FABRIC_ZONE_SCOPED_N("DebrisPool::update");

    while (!pending_.empty() && debris_.size() < maxActive_) {
        auto& p = pending_.front();
        debris_.push_back({p.position, Vector3<float, Space::World>(0, 0, 0), p.density, p.radius,
                           particleConvertLifetime_, false, 0});
        pending_.pop_front();
    }

    updateSleeping(dt);
    mergeNearby();
    convertToParticles();

    auto it = debris_.begin();
    while (it != debris_.end()) {
        if (!it->sleeping) {
            it->velocity.y -= 9.81f * dt;
            it->position = it->position + it->velocity * dt;

            float groundLevel = 0.0f;
            if (it->position.y < groundLevel) {
                it->position.y = groundLevel;
                it->velocity.y = -it->velocity.y * 0.3f;
                it->velocity.x *= 0.8f;
                it->velocity.z *= 0.8f;

                if (std::abs(it->velocity.y) < 0.1f) {
                    it->velocity.y = 0.0f;
                }
            }
        }

        it->lifetime -= dt;
        if (it->lifetime <= 0.0f) {
            it = debris_.erase(it);
        } else {
            ++it;
        }
    }
}

void DebrisPool::clear() {
    debris_.clear();
    pending_.clear();
}

size_t DebrisPool::activeCount() const {
    return debris_.size();
}

size_t DebrisPool::maxActive() const {
    return maxActive_;
}

void DebrisPool::setMaxActive(size_t max) {
    maxActive_ = max;
    while (debris_.size() > maxActive_) {
        debris_.pop_back();
    }
}

void DebrisPool::setMergeDistance(float distance) {
    mergeDistance_ = distance;
}

void DebrisPool::setSleepThreshold(float velocityThreshold) {
    sleepThreshold_ = velocityThreshold;
}

void DebrisPool::setSleepFrames(int frames) {
    sleepFrames_ = frames;
}

void DebrisPool::setParticleEmitter(ParticleEmitter emitter) {
    particleEmitter_ = std::move(emitter);
}

void DebrisPool::enableParticleConversion(bool enable) {
    particleConversionEnabled_ = enable;
}

void DebrisPool::setParticleConvertLifetime(float lifetime) {
    particleConvertLifetime_ = lifetime;
}

const std::vector<Debris>& DebrisPool::getDebris() const {
    return debris_;
}

DebrisPool::CellKey DebrisPool::toCell(const Vector3<float, Space::World>& pos, float invCellSize) {
    return {static_cast<int32_t>(std::floor(pos.x * invCellSize)),
            static_cast<int32_t>(std::floor(pos.y * invCellSize)),
            static_cast<int32_t>(std::floor(pos.z * invCellSize))};
}

void DebrisPool::mergeNearby() {
    const size_t count = debris_.size();
    if (count < 2)
        return;

    float maxRadius = 0.0f;
    for (const auto& d : debris_) {
        if (d.radius > maxRadius)
            maxRadius = d.radius;
    }

    const float cellSize = mergeDistance_ + 2.0f * maxRadius;
    if (cellSize <= 0.0f)
        return;
    const float invCellSize = 1.0f / cellSize;

    std::unordered_map<CellKey, std::vector<size_t>, CellKeyHash> grid;
    grid.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        if (!debris_[i].sleeping) {
            grid[toCell(debris_[i].position, invCellSize)].push_back(i);
        }
    }

    std::vector<bool> dead(count, false);

    for (size_t i = 0; i < count; ++i) {
        if (dead[i] || debris_[i].sleeping)
            continue;

        CellKey base = toCell(debris_[i].position, invCellSize);

        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dy = -1; dy <= 1; ++dy) {
                for (int32_t dz = -1; dz <= 1; ++dz) {
                    CellKey neighbor{base.x + dx, base.y + dy, base.z + dz};
                    auto it = grid.find(neighbor);
                    if (it == grid.end())
                        continue;

                    for (size_t j : it->second) {
                        if (j <= i || dead[j] || debris_[j].sleeping)
                            continue;

                        if (shouldMerge(debris_[i], debris_[j])) {
                            debris_[i] = merge(debris_[i], debris_[j]);
                            dead[j] = true;
                        }
                    }
                }
            }
        }
    }

    // Compact dead entries (replaces erase with O(n) single-pass)
    size_t write = 0;
    for (size_t read = 0; read < debris_.size(); ++read) {
        if (!dead[read]) {
            if (write != read) {
                debris_[write] = std::move(debris_[read]);
            }
            ++write;
        }
    }
    debris_.resize(write);
}

void DebrisPool::updateSleeping(float dt) {
    for (auto& d : debris_) {
        float speed = d.velocity.length();

        if (d.sleeping) {
            if (speed > sleepThreshold_ * 2.0f) {
                d.sleeping = false;
                d.sleepFrames = 0;
            }
        } else {
            if (speed < sleepThreshold_) {
                d.sleepFrames++;
                if (d.sleepFrames >= sleepFrames_) {
                    d.sleeping = true;
                    d.velocity = Vector3<float, Space::World>(0, 0, 0);
                }
            } else {
                d.sleepFrames = 0;
            }
        }
    }
}

void DebrisPool::convertToParticles() {
    if (!particleConversionEnabled_ || !particleEmitter_) {
        return;
    }

    for (auto it = debris_.begin(); it != debris_.end();) {
        if (it->lifetime <= particleConvertLifetime_ * 0.5f && !it->sleeping) {
            int particleCount = static_cast<int>(it->density * 10.0f);
            particleEmitter_(it->position, it->radius, particleCount);
            it = debris_.erase(it);
        } else {
            ++it;
        }
    }
}

bool DebrisPool::shouldMerge(const Debris& a, const Debris& b) const {
    float distSq = (a.position - b.position).lengthSquared();
    float threshold = a.radius + b.radius + mergeDistance_;
    return distSq < threshold * threshold;
}

Debris DebrisPool::merge(const Debris& a, const Debris& b) const {
    float totalDensity = a.density + b.density;
    float totalVolume = std::pow(a.radius, 3.0f) + std::pow(b.radius, 3.0f);
    float newRadius = std::cbrt(totalVolume);

    Vector3<float, Space::World> newPosition =
        (a.position * a.density + b.position * b.density) * (1.0f / totalDensity);

    Vector3<float, Space::World> newVelocity =
        (a.velocity * a.density + b.velocity * b.density) * (1.0f / totalDensity);

    return {newPosition, newVelocity, totalDensity, newRadius, std::min(a.lifetime, b.lifetime), false, 0};
}

} // namespace fabric

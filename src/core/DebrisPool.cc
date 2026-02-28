#include "fabric/core/DebrisPool.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <cmath>

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

void DebrisPool::mergeNearby() {
    for (size_t i = 0; i < debris_.size();) {
        if (debris_[i].sleeping) {
            ++i;
            continue;
        }

        bool merged = false;
        for (size_t j = i + 1; j < debris_.size();) {
            if (debris_[j].sleeping) {
                ++j;
                continue;
            }

            if (shouldMerge(debris_[i], debris_[j])) {
                debris_[i] = merge(debris_[i], debris_[j]);
                debris_.erase(debris_.begin() + static_cast<ptrdiff_t>(j));
                merged = true;
            } else {
                ++j;
            }
        }

        if (merged) {
            continue;
        }
        ++i;
    }
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
    float dist = (a.position - b.position).length();
    return dist < (a.radius + b.radius + mergeDistance_);
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

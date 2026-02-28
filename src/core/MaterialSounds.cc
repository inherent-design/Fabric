#include "fabric/core/MaterialSounds.hh"

#include <algorithm>
#include <cmath>

namespace fabric {

MaterialSounds::MaterialSounds() : rng_(std::random_device{}()) {}

MaterialSounds::MaterialSounds(uint32_t seed) : rng_(seed) {}

void MaterialSounds::registerMaterial(MaterialType type, MaterialSoundSet sounds) {
    registry_[type] = std::move(sounds);
}

MaterialType MaterialSounds::mapEssenceToMaterial(const Essence& e) {
    // Essence is a vec4 (r, g, b, a) representing material properties.
    // Simple threshold classifier on dominant color channels (order matters):
    //   1. White/bright (r > 0.8, g > 0.8, b > 0.8)          -> Snow
    //   2. Blue-dominant (b > 0.6, b > r, b > g)             -> Water
    //   3. Metallic (alpha > 0.8, gray: |r-g| < 0.1, |g-b| < 0.1) -> Metal
    //   4. Yellow-warm (r > 0.6, g > 0.5, b < 0.3)           -> Sand
    //   5. Green-dominant (g > 0.5, g > r, g > b)            -> Grass
    //   6. Brown (r > 0.4, g > 0.2, b < 0.2, r > g)         -> Dirt
    //   7. Brown-green (r [0.3,0.7], g [0.2,0.5], b < 0.2)  -> Wood
    //   8. Dark/neutral gray (r < 0.5, g < 0.5, b < 0.5)    -> Stone
    //   9. Fallback                                           -> Default

    float r = e.x;
    float g = e.y;
    float b = e.z;
    float a = e.w;

    // Snow: bright white
    if (r > 0.8f && g > 0.8f && b > 0.8f) {
        return MaterialType::Snow;
    }

    // Water: blue-dominant
    if (b > 0.6f && b > r && b > g) {
        return MaterialType::Water;
    }

    // Metal: high alpha + gray (channels close together)
    if (a > 0.8f && std::abs(r - g) < 0.1f && std::abs(g - b) < 0.1f) {
        return MaterialType::Metal;
    }

    // Sand: warm yellow
    if (r > 0.6f && g > 0.5f && b < 0.3f) {
        return MaterialType::Sand;
    }

    // Grass: green-dominant
    if (g > 0.5f && g > r && g > b) {
        return MaterialType::Grass;
    }

    // Dirt: brown (r > g, low blue)
    if (r > 0.4f && g > 0.2f && b < 0.2f && r > g) {
        return MaterialType::Dirt;
    }

    // Wood: moderate brown-green (r in mid range, some green, low blue)
    if (r > 0.3f && r < 0.7f && g > 0.2f && g < 0.5f && b < 0.2f) {
        return MaterialType::Wood;
    }

    // Stone: dark or neutral gray
    if (r < 0.5f && g < 0.5f && b < 0.5f) {
        return MaterialType::Stone;
    }

    return MaterialType::Default;
}

std::string MaterialSounds::getFootstepSound(MaterialType type) {
    auto it = registry_.find(type);
    if (it == registry_.end()) {
        return {};
    }
    return pickSound(it->second.footstepSounds, type, lastFootstep_);
}

std::string MaterialSounds::getImpactSound(MaterialType type) {
    auto it = registry_.find(type);
    if (it == registry_.end()) {
        return {};
    }
    return pickSound(it->second.impactSounds, type, lastImpact_);
}

MaterialType MaterialSounds::detectSurfaceBelow(const ChunkedGrid<float>& density, const ChunkedGrid<Essence>& essence,
                                                float x, float y, float z) {
    // Cast a short downward ray (maxDistance = 2.0 voxels)
    auto hit = castRay(density, x, y, z, 0.0f, -1.0f, 0.0f, 2.0f);
    if (!hit.has_value()) {
        return MaterialType::Default;
    }

    Essence voxelEssence = essence.get(hit->x, hit->y, hit->z);
    return mapEssenceToMaterial(voxelEssence);
}

std::string MaterialSounds::pickSound(const std::vector<std::string>& sounds, MaterialType type,
                                      std::unordered_map<MaterialType, std::string>& lastPlayed) {
    if (sounds.empty()) {
        return {};
    }

    if (sounds.size() == 1) {
        lastPlayed[type] = sounds[0];
        return sounds[0];
    }

    // Pick randomly, excluding the last-played sound for this type
    const std::string& last = lastPlayed[type];
    std::vector<size_t> candidates;
    candidates.reserve(sounds.size());
    for (size_t i = 0; i < sounds.size(); ++i) {
        if (sounds[i] != last) {
            candidates.push_back(i);
        }
    }

    // Shouldn't happen unless all entries are identical, but handle gracefully
    if (candidates.empty()) {
        size_t idx = std::uniform_int_distribution<size_t>(0, sounds.size() - 1)(rng_);
        lastPlayed[type] = sounds[idx];
        return sounds[idx];
    }

    size_t pick = std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(rng_);
    const std::string& chosen = sounds[candidates[pick]];
    lastPlayed[type] = chosen;
    return chosen;
}

} // namespace fabric

#include "fabric/core/LSystemVegetation.hh"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <random>
#include <stack>

namespace fabric {

// ---------- expand ----------

std::string expand(const LSystemRule& rule) {
    std::string current = rule.axiom;

    for (int i = 0; i < rule.iterations; ++i) {
        std::string next;
        next.reserve(current.size() * 2); // heuristic pre-allocation

        for (char c : current) {
            auto it = rule.rules.find(c);
            if (it != rule.rules.end()) {
                next.append(it->second);
            } else {
                next.push_back(c);
            }
        }

        current = std::move(next);
    }

    return current;
}

// ---------- interpret ----------

namespace {

/// Turtle state for push/pop operations.
struct TurtleState {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 heading{0.0f, 1.0f, 0.0f}; // forward (up = +Y default)
    glm::vec3 left{-1.0f, 0.0f, 0.0f};   // left axis
    glm::vec3 up{0.0f, 0.0f, 1.0f};      // up axis (perpendicular to heading)
    float radius = 1.0f;
    int materialTag = 0;
};

/// Rotate a vector around an axis by an angle in radians.
glm::vec3 rotateAround(const glm::vec3& v, const glm::vec3& axis, float angleRad) {
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    float t = 1.0f - c;
    glm::vec3 a = glm::normalize(axis);

    // Rodrigues' rotation formula
    return v * c + glm::cross(a, v) * s + a * glm::dot(a, v) * t;
}

} // anonymous namespace

std::vector<TurtleSegment> interpret(const std::string& expanded, const LSystemRule& params) {
    std::vector<TurtleSegment> segments;
    segments.reserve(expanded.size() / 2); // heuristic

    float angleRad = params.angle * std::numbers::pi_v<float> / 180.0f;

    TurtleState turtle;
    turtle.radius = params.segmentLength; // initial radius proportional to segment length
    std::stack<TurtleState> stateStack;

    for (char c : expanded) {
        switch (c) {
            case 'F': {
                // Move forward, creating a segment.
                glm::vec3 start = turtle.position;
                turtle.position += turtle.heading * params.segmentLength;

                TurtleSegment seg;
                seg.start = start;
                seg.end = turtle.position;
                seg.radius = turtle.radius;
                seg.materialTag = turtle.materialTag;
                segments.push_back(seg);
                break;
            }

            case 'f': {
                // Move forward without creating a segment.
                turtle.position += turtle.heading * params.segmentLength;
                break;
            }

            case '+': {
                // Yaw left: rotate heading and left around up axis.
                turtle.heading = rotateAround(turtle.heading, turtle.up, angleRad);
                turtle.left = rotateAround(turtle.left, turtle.up, angleRad);
                break;
            }

            case '-': {
                // Yaw right: rotate heading and left around up axis (negative).
                turtle.heading = rotateAround(turtle.heading, turtle.up, -angleRad);
                turtle.left = rotateAround(turtle.left, turtle.up, -angleRad);
                break;
            }

            case '^': {
                // Pitch up: rotate heading and up around left axis.
                turtle.heading = rotateAround(turtle.heading, turtle.left, angleRad);
                turtle.up = rotateAround(turtle.up, turtle.left, angleRad);
                break;
            }

            case '&': {
                // Pitch down: rotate heading and up around left axis (negative).
                turtle.heading = rotateAround(turtle.heading, turtle.left, -angleRad);
                turtle.up = rotateAround(turtle.up, turtle.left, -angleRad);
                break;
            }

            case '\\': {
                // Roll left: rotate left and up around heading axis.
                turtle.left = rotateAround(turtle.left, turtle.heading, angleRad);
                turtle.up = rotateAround(turtle.up, turtle.heading, angleRad);
                break;
            }

            case '/': {
                // Roll right: rotate left and up around heading axis (negative).
                turtle.left = rotateAround(turtle.left, turtle.heading, -angleRad);
                turtle.up = rotateAround(turtle.up, turtle.heading, -angleRad);
                break;
            }

            case '[': {
                // Push state and decay radius.
                stateStack.push(turtle);
                turtle.radius *= params.radiusDecay;
                break;
            }

            case ']': {
                // Pop state.
                if (!stateStack.empty()) {
                    turtle = stateStack.top();
                    stateStack.pop();
                }
                break;
            }

            case 'L': {
                // Switch to leaf material.
                turtle.materialTag = 1;
                break;
            }

            default:
                // Unknown characters are ignored (e.g. 'X' used as variable).
                break;
        }
    }

    return segments;
}

// ---------- voxelizeSegment ----------

void voxelizeSegment(const TurtleSegment& seg, DensityField& density, EssenceField& essence) {
    // Choose essence by material tag.
    auto ess = (seg.materialTag == 0) ? kWoodEssence : kLeafEssence;

    glm::vec3 delta = seg.end - seg.start;
    float length = glm::length(delta);

    if (length < 1e-6f) {
        // Degenerate segment: stamp a single voxel at start.
        int ix = static_cast<int>(std::floor(seg.start.x));
        int iy = static_cast<int>(std::floor(seg.start.y));
        int iz = static_cast<int>(std::floor(seg.start.z));
        float d = density.read(ix, iy, iz);
        density.write(ix, iy, iz, std::clamp(d + 1.0f, 0.0f, 1.0f));
        essence.write(ix, iy, iz, ess);
        return;
    }

    glm::vec3 dir = delta / length;

    // Number of steps: at least 1 per voxel along the line.
    int steps = static_cast<int>(std::ceil(length)) + 1;
    float stepSize = length / static_cast<float>(steps);
    int iRadius = std::max(0, static_cast<int>(std::ceil(seg.radius)) - 1);

    for (int s = 0; s <= steps; ++s) {
        float t = static_cast<float>(s) * stepSize;
        glm::vec3 pos = seg.start + dir * t;

        int cx = static_cast<int>(std::floor(pos.x));
        int cy = static_cast<int>(std::floor(pos.y));
        int cz = static_cast<int>(std::floor(pos.z));

        // Fill sphere cross-section at each step.
        for (int dz = -iRadius; dz <= iRadius; ++dz) {
            for (int dy = -iRadius; dy <= iRadius; ++dy) {
                for (int dx = -iRadius; dx <= iRadius; ++dx) {
                    float dist2 = static_cast<float>(dx * dx + dy * dy + dz * dz);
                    if (dist2 <= seg.radius * seg.radius) {
                        int vx = cx + dx;
                        int vy = cy + dy;
                        int vz = cz + dz;

                        // Distance-based falloff for density.
                        float dist = std::sqrt(dist2);
                        float contribution = 1.0f - (dist / (seg.radius + 1.0f));
                        float d = density.read(vx, vy, vz);
                        density.write(vx, vy, vz, std::clamp(d + contribution, 0.0f, 1.0f));
                        essence.write(vx, vy, vz, ess);
                    }
                }
            }
        }
    }
}

// ---------- voxelizeTree ----------

void voxelizeTree(const std::vector<TurtleSegment>& segments, DensityField& density, EssenceField& essence,
                  const glm::ivec3& origin) {
    for (const auto& seg : segments) {
        TurtleSegment offset = seg;
        offset.start += glm::vec3(origin);
        offset.end += glm::vec3(origin);
        voxelizeSegment(offset, density, essence);
    }
}

// ---------- VegetationPlacer ----------

VegetationPlacer::VegetationPlacer(const VegetationConfig& config) : config_(config) {}

const VegetationConfig& VegetationPlacer::config() const {
    return config_;
}

void VegetationPlacer::setConfig(const VegetationConfig& config) {
    config_ = config;
}

void VegetationPlacer::generate(DensityField& density, EssenceField& essence, const AABB& region) {
    // Resolve species list: use presets if none specified.
    std::vector<LSystemRule> species = config_.species;
    if (species.empty()) {
        species = {kBushRule, kSmallTreeRule, kLargeTreeRule};
    }

    // Compute integer bounds from AABB (same as TerrainGenerator: floor min, ceil max).
    int minX = static_cast<int>(std::floor(region.min.x));
    int minY = static_cast<int>(std::floor(region.min.y));
    int minZ = static_cast<int>(std::floor(region.min.z));
    int maxX = static_cast<int>(std::ceil(region.max.x));
    int maxY = static_cast<int>(std::ceil(region.max.y));
    int maxZ = static_cast<int>(std::ceil(region.max.z));

    if (maxX <= minX || maxY <= minY || maxZ <= minZ) {
        return;
    }

    float spacing = std::max(config_.spacing, 1.0f);

    // Grid-based spacing enforcement: divide the (x, z) plane into spacing x spacing cells.
    // At most one tree per cell.
    int cellMinX = static_cast<int>(std::floor(static_cast<float>(minX) / spacing));
    int cellMaxX = static_cast<int>(std::ceil(static_cast<float>(maxX) / spacing));
    int cellMinZ = static_cast<int>(std::floor(static_cast<float>(minZ) / spacing));
    int cellMaxZ = static_cast<int>(std::ceil(static_cast<float>(maxZ) / spacing));

    for (int cz = cellMinZ; cz < cellMaxZ; ++cz) {
        for (int cx = cellMinX; cx < cellMaxX; ++cx) {
            // Deterministic PRNG per cell.
            std::size_t cellHash = std::hash<int>{}(cx) ^ (std::hash<int>{}(cz) << 16);
            std::mt19937 rng(static_cast<unsigned>(config_.seed ^ static_cast<int>(cellHash)));
            std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

            // Pick a random (x, z) position within the cell.
            float fx = (static_cast<float>(cx) + dist01(rng)) * spacing;
            float fz = (static_cast<float>(cz) + dist01(rng)) * spacing;

            int x = static_cast<int>(std::floor(fx));
            int z = static_cast<int>(std::floor(fz));

            // Skip if outside the actual region bounds.
            if (x < minX || x >= maxX || z < minZ || z >= maxZ) {
                continue;
            }

            // Surface detection: scan Y from top to bottom in this column.
            int surfaceY = -1;
            for (int y = maxY - 1; y >= minY; --y) {
                float d = density.read(x, y, z);
                if (d >= config_.surfaceThreshold) {
                    // Check if this is the surface: y+1 is either above region or below threshold.
                    if (y + 1 >= maxY || density.read(x, y + 1, z) < config_.surfaceThreshold) {
                        surfaceY = y;
                        break;
                    }
                }
            }

            if (surfaceY < 0) {
                continue; // No surface found in this column.
            }

            // Species selection via PRNG.
            std::uniform_int_distribution<int> speciesDist(0, static_cast<int>(species.size()) - 1);
            int speciesIdx = speciesDist(rng);

            // Generate tree: expand L-system, interpret, voxelize.
            const auto& rule = species[static_cast<size_t>(speciesIdx)];
            std::string expanded = expand(rule);
            auto segments = interpret(expanded, rule);
            voxelizeTree(segments, density, essence, glm::ivec3(x, surfaceY + 1, z));
        }
    }
}

} // namespace fabric

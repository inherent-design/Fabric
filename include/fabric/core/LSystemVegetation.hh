#pragma once

#include <map>
#include <random>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Rendering.hh"

namespace fabric {

// ---- Essence constants for voxelization ----

/// Wood essence: brown channel encoding (x=0.6, y=0.3, z=0.1, w=1.0).
inline const Vector4<float, Space::World> kWoodEssence{0.6f, 0.3f, 0.1f, 1.0f};

/// Leaf essence: green channel encoding (x=0.2, y=0.7, z=0.1, w=1.0).
inline const Vector4<float, Space::World> kLeafEssence{0.2f, 0.7f, 0.1f, 1.0f};

/// Rule set defining an L-system grammar and turtle interpretation parameters.
struct LSystemRule {
    std::string axiom;                 ///< Initial string (e.g. "F" or "X").
    std::map<char, std::string> rules; ///< Production rules: char -> replacement.
    int iterations = 3;                ///< Number of rewriting iterations.
    float angle = 25.0f;               ///< Default turning angle in degrees.
    float segmentLength = 1.0f;        ///< Length of each 'F' step.
    float radiusDecay = 0.7f;          ///< Multiplicative radius decay per '[' push.
};

/// A line segment produced by turtle interpretation.
struct TurtleSegment {
    glm::vec3 start{0.0f}; ///< Segment start position.
    glm::vec3 end{0.0f};   ///< Segment end position.
    float radius = 1.0f;   ///< Segment radius (decays along branches).
    int materialTag = 0;   ///< 0 = wood, 1 = leaf.
};

// ---- Built-in presets ----

/// Bushy shrub: short segments, wide branching angle, 3 iterations.
inline const LSystemRule kBushRule = {
    "F",                             // axiom
    {{'F', "FF+[+F-F-F]-[-F+F+F]"}}, // rules
    3,                               // iterations
    25.0f,                           // angle (degrees)
    0.5f,                            // segmentLength
    0.75f                            // radiusDecay
};

/// Small deciduous tree: medium segments, moderate angle, 4 iterations.
inline const LSystemRule kSmallTreeRule = {
    "X",                                 // axiom
    {{'X', "F[+X][-X]FX"}, {'F', "FF"}}, // rules
    4,                                   // iterations
    22.0f,                               // angle (degrees)
    1.0f,                                // segmentLength
    0.7f                                 // radiusDecay
};

/// Large tree: longer segments, narrow angle, 5 iterations.
inline const LSystemRule kLargeTreeRule = {
    "X",                                  // axiom
    {{'X', "F[+X]F[-X]+X"}, {'F', "FF"}}, // rules
    5,                                    // iterations
    20.0f,                                // angle (degrees)
    2.0f,                                 // segmentLength
    0.65f                                 // radiusDecay
};

/// Expand an L-system grammar by applying production rules for N iterations.
/// Returns the fully expanded string.
std::string expand(const LSystemRule& rule);

/// Interpret an expanded L-system string using a 3D turtle.
/// Produces line segments with position, radius, and material tags.
///
/// Turtle commands:
///   F  - move forward, creating a segment
///   f  - move forward without creating a segment
///   +  - yaw left (rotate around up axis by +angle)
///   -  - yaw right (rotate around up axis by -angle)
///   ^  - pitch up (rotate around right axis by +angle)
///   &  - pitch down (rotate around right axis by -angle)
///   \  - roll left (rotate around forward axis by +angle)
///   /  - roll right (rotate around forward axis by -angle)
///   [  - push turtle state (position, orientation, radius)
///   ]  - pop turtle state
///   L  - switch material tag to "leaf" (1)
std::vector<TurtleSegment> interpret(const std::string& expanded, const LSystemRule& params);

// ---- Voxelization ----

/// Rasterize a single turtle segment into density and essence fields using 3D DDA
/// thick-line traversal. Maps materialTag 0 -> kWoodEssence, 1 -> kLeafEssence.
/// Density is clamped to [0, 1].
void voxelizeSegment(const TurtleSegment& seg, DensityField& density, EssenceField& essence);

/// Rasterize an entire tree (vector of segments) into density and essence fields.
/// Each segment is offset by the given origin before voxelization.
void voxelizeTree(const std::vector<TurtleSegment>& segments, DensityField& density, EssenceField& essence,
                  const glm::ivec3& origin);

// ---- Vegetation placement ----

/// Configuration for the VegetationPlacer pipeline stage.
struct VegetationConfig {
    int seed = 42;                    ///< PRNG seed for deterministic placement.
    float surfaceThreshold = 0.5f;    ///< Density at or above this value is considered surface.
    float spacing = 8.0f;             ///< Minimum distance between tree origins (grid cell size).
    std::vector<LSystemRule> species; ///< Species to place. If empty, uses preset defaults.
};

/// Places L-system vegetation onto a terrain surface within a given AABB region.
/// Follows the TerrainGenerator / CaveCarver config + class + generate pattern.
class VegetationPlacer {
  public:
    explicit VegetationPlacer(const VegetationConfig& config);

    /// Generate vegetation in the given region. Scans density for surface,
    /// places trees at deterministic positions, voxelizes into fields.
    void generate(DensityField& density, EssenceField& essence, const AABB& region);

    const VegetationConfig& config() const;
    void setConfig(const VegetationConfig& config);

  private:
    VegetationConfig config_;
};

} // namespace fabric

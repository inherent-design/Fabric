#pragma once

#include "fabric/core/Spatial.hh"
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <ozz/animation/runtime/skeleton.h>

namespace fabric {

// Monotonically increasing ID for mesh identity (cache keys, lookup).
// Each call returns a unique non-zero value, safe across translation units.
inline uint64_t nextMeshId() {
    static std::atomic<uint64_t> counter{0};
    return ++counter;
}

// Joint hierarchy entry for skeleton data
struct JointInfo {
    std::string name;
    int parentIndex = -1; // -1 for root joints
    std::array<float, 16> inverseBindMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

// Loaded mesh data from glTF 2.0 file
struct MeshData {
    // Stable identity for cache keying (survives moves/reallocs)
    uint64_t id = nextMeshId();

    // Geometry
    std::vector<Vector3<float, Space::Local>> positions;
    std::vector<Vector3<float, Space::Local>> normals;
    std::vector<Vector2<float, Space::Local>> uvs;
    std::vector<uint32_t> indices;

    // Skinning (optional; empty if mesh has no skin)
    std::vector<std::array<uint16_t, 4>> jointIndices;
    std::vector<Vector4<float, Space::Local>> jointWeights;

    // Skeleton hierarchy (empty if no skin)
    std::vector<JointInfo> skeleton;
};

// Loads glTF 2.0 mesh data using fastgltf. Supports skinned meshes with
// up to 100 joints for humanoid characters.
class MeshLoader {
  public:
    MeshData load(const std::filesystem::path& path);
};

// Convert MeshData joint hierarchy to ozz runtime skeleton.
// Returns nullptr if joints vector is empty.
std::shared_ptr<ozz::animation::Skeleton> buildOzzSkeleton(const std::vector<JointInfo>& joints);

} // namespace fabric

#pragma once

#include "fabric/core/Spatial.hh"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <ozz/animation/runtime/skeleton.h>

namespace fabric {

// Joint hierarchy entry for skeleton data
struct JointInfo {
    std::string name;
    int parentIndex = -1; // -1 for root joints
    std::array<float, 16> inverseBindMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

// Loaded mesh data from glTF 2.0 file
struct MeshData {
    // Stable identity for cache keying (survives moves/copies).
    // Assigned by MeshLoader::load(); default 0 for test-constructed instances.
    uint64_t id = 0;

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

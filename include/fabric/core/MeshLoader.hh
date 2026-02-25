#pragma once

#include "fabric/core/Spatial.hh"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fabric {

// Joint hierarchy entry for skeleton data
struct JointInfo {
    std::string name;
    int parentIndex = -1; // -1 for root joints
    std::array<float, 16> inverseBindMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

// Loaded mesh data from glTF 2.0 file
struct MeshData {
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

} // namespace fabric

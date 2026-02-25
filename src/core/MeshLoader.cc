#include "fabric/core/MeshLoader.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

namespace fabric {

MeshData MeshLoader::load(const std::filesystem::path& path) {
    FABRIC_ZONE_SCOPED;

    if (!std::filesystem::exists(path)) {
        throwError("MeshLoader::load: file not found: " + path.string());
    }

    auto bufferExpected = fastgltf::GltfDataBuffer::FromPath(path);
    if (bufferExpected.error() != fastgltf::Error::None) {
        throwError("MeshLoader::load: failed to read file: " + path.string());
    }
    auto& dataBuffer = bufferExpected.get();

    fastgltf::Parser parser;

    auto type = fastgltf::determineGltfFileType(dataBuffer);
    if (type == fastgltf::GltfType::Invalid) {
        throwError("MeshLoader::load: invalid glTF file: " + path.string());
    }

    auto expected = parser.loadGltf(dataBuffer, path.parent_path(), fastgltf::Options::LoadExternalBuffers);
    if (expected.error() != fastgltf::Error::None) {
        throwError("MeshLoader::load: failed to parse glTF: " + path.string());
    }

    fastgltf::Asset& asset = expected.get();

    if (asset.meshes.empty()) {
        throwError("MeshLoader::load: no meshes in file: " + path.string());
    }

    MeshData result;

    // Load first mesh, first primitive
    const auto& mesh = asset.meshes[0];
    if (mesh.primitives.empty()) {
        throwError("MeshLoader::load: mesh has no primitives: " + path.string());
    }

    const auto& primitive = mesh.primitives[0];

    // Positions (required)
    {
        auto it = primitive.findAttribute("POSITION");
        if (it == primitive.attributes.cend()) {
            throwError("MeshLoader::load: no POSITION attribute");
        }
        const auto& accessor = asset.accessors[it->accessorIndex];
        result.positions.resize(accessor.count);
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
            asset, accessor, [&](fastgltf::math::fvec3 v, std::size_t i) {
                result.positions[i] = Vector3<float, Space::Local>(v.x(), v.y(), v.z());
            });
    }

    // Normals (optional)
    {
        auto it = primitive.findAttribute("NORMAL");
        if (it != primitive.attributes.cend()) {
            const auto& accessor = asset.accessors[it->accessorIndex];
            result.normals.resize(accessor.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                asset, accessor, [&](fastgltf::math::fvec3 v, std::size_t i) {
                    result.normals[i] = Vector3<float, Space::Local>(v.x(), v.y(), v.z());
                });
        }
    }

    // UVs (optional)
    {
        auto it = primitive.findAttribute("TEXCOORD_0");
        if (it != primitive.attributes.cend()) {
            const auto& accessor = asset.accessors[it->accessorIndex];
            result.uvs.resize(accessor.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                asset, accessor, [&](fastgltf::math::fvec2 v, std::size_t i) {
                    result.uvs[i] = Vector2<float, Space::Local>(v.x(), v.y());
                });
        }
    }

    // Indices (optional)
    if (primitive.indicesAccessor.has_value()) {
        const auto& accessor = asset.accessors[*primitive.indicesAccessor];
        result.indices.resize(accessor.count);
        fastgltf::iterateAccessorWithIndex<uint32_t>(asset, accessor,
                                                     [&](uint32_t val, std::size_t i) { result.indices[i] = val; });
    }

    // Joint indices (optional, JOINTS_0)
    {
        auto it = primitive.findAttribute("JOINTS_0");
        if (it != primitive.attributes.cend()) {
            const auto& accessor = asset.accessors[it->accessorIndex];
            result.jointIndices.resize(accessor.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec4>(
                asset, accessor, [&](fastgltf::math::u16vec4 v, std::size_t i) {
                    result.jointIndices[i] = {v.x(), v.y(), v.z(), v.w()};
                });
        }
    }

    // Joint weights (optional, WEIGHTS_0)
    {
        auto it = primitive.findAttribute("WEIGHTS_0");
        if (it != primitive.attributes.cend()) {
            const auto& accessor = asset.accessors[it->accessorIndex];
            result.jointWeights.resize(accessor.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                asset, accessor, [&](fastgltf::math::fvec4 v, std::size_t i) {
                    result.jointWeights[i] = Vector4<float, Space::Local>(v.x(), v.y(), v.z(), v.w());
                });
        }
    }

    // Skeleton: load first skin if present
    if (!asset.skins.empty()) {
        const auto& skin = asset.skins[0];

        // Build joint info from skin joints
        result.skeleton.resize(skin.joints.size());
        for (std::size_t i = 0; i < skin.joints.size(); ++i) {
            std::size_t nodeIndex = skin.joints[i];
            const auto& node = asset.nodes[nodeIndex];
            result.skeleton[i].name = std::string(node.name);

            // Find parent by checking if any other joint's node is this node's parent
            result.skeleton[i].parentIndex = -1;
            for (std::size_t j = 0; j < skin.joints.size(); ++j) {
                const auto& parentNode = asset.nodes[skin.joints[j]];
                for (auto childIdx : parentNode.children) {
                    if (childIdx == nodeIndex && j != i) {
                        result.skeleton[i].parentIndex = static_cast<int>(j);
                    }
                }
            }
        }

        // Load inverse bind matrices if present
        if (skin.inverseBindMatrices.has_value()) {
            const auto& accessor = asset.accessors[*skin.inverseBindMatrices];
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(
                asset, accessor, [&](const fastgltf::math::fmat4x4& m, std::size_t i) {
                    if (i < result.skeleton.size()) {
                        // fastgltf fmat4x4 is column-major; copy directly
                        for (int col = 0; col < 4; ++col) {
                            for (int row = 0; row < 4; ++row) {
                                result.skeleton[i].inverseBindMatrix[static_cast<size_t>(col * 4 + row)] =
                                    m.col(col)[row];
                            }
                        }
                    }
                });
        }
    }

    return result;
}

} // namespace fabric

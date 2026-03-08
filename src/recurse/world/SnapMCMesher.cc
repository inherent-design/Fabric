#include "recurse/world/SnapMCMesher.hh"

#include "recurse/world/SnapMCTables.hh"

#include <algorithm>
#include <cmath>
#include <unordered_map>

using fabric::kChunkSize;

namespace recurse {

// -- Merge helper -------------------------------------------------------------

namespace {

struct QuantizedPos {
    int32_t x, y, z;
    bool operator==(const QuantizedPos&) const = default;
};

struct QuantizedPosHash {
    size_t operator()(const QuantizedPos& p) const {
        auto h = std::hash<int32_t>{};
        return h(p.x) ^ (h(p.y) * 2654435761u) ^ (h(p.z) * 40503u);
    }
};

void mergeCoincidentVertices(SmoothChunkMeshData& mesh, float epsilon) {
    if (mesh.vertices.empty())
        return;

    float invEps = 1.0f / std::max(epsilon, 1e-7f);

    std::unordered_map<QuantizedPos, uint32_t, QuantizedPosHash> posMap;
    std::vector<uint32_t> remap(mesh.vertices.size());
    std::vector<SmoothVoxelVertex> newVerts;
    newVerts.reserve(mesh.vertices.size());

    for (uint32_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        QuantizedPos qp{
            static_cast<int32_t>(std::round(v.px * invEps)),
            static_cast<int32_t>(std::round(v.py * invEps)),
            static_cast<int32_t>(std::round(v.pz * invEps)),
        };

        auto [it, inserted] = posMap.try_emplace(qp, static_cast<uint32_t>(newVerts.size()));
        if (inserted) {
            newVerts.push_back(v);
        }
        remap[i] = it->second;
    }

    std::vector<uint32_t> newIndices;
    newIndices.reserve(mesh.indices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint32_t a = remap[mesh.indices[i]];
        uint32_t b = remap[mesh.indices[i + 1]];
        uint32_t c = remap[mesh.indices[i + 2]];
        if (a != b && b != c && a != c) {
            newIndices.push_back(a);
            newIndices.push_back(b);
            newIndices.push_back(c);
        }
    }

    mesh.vertices = std::move(newVerts);
    mesh.indices = std::move(newIndices);
}

} // namespace

// -- SnapMCMesher -------------------------------------------------------------

SnapMCMesher::SnapMCMesher() : config_() {}
SnapMCMesher::SnapMCMesher(const Config& config) : config_(config) {}

SmoothChunkMeshData SnapMCMesher::meshChunk(const ChunkDensityCache& density, const ChunkMaterialCache& material,
                                            float isovalue, int lodLevel) {
    const int stride = 1 << lodLevel;
    const int gridSize = (kChunkSize / stride) + 1;

    SmoothChunkMeshData output;
    output.vertices.reserve(4096);
    output.indices.reserve(8192);

    // Per-cell edge vertex indices (reused each cell, 12 edges max)
    uint32_t edgeVerts[12];

    for (int cz = 0; cz < gridSize - 1; ++cz) {
        for (int cy = 0; cy < gridSize - 1; ++cy) {
            for (int cx = 0; cx < gridSize - 1; ++cx) {
                // Sample 8 corners from the density cache
                uint8_t cubeIndex = 0;
                float cornerVals[8];

                for (int i = 0; i < 8; ++i) {
                    int lx = cx * stride + 1 + kCornerOffsets[i][0] * stride;
                    int ly = cy * stride + 1 + kCornerOffsets[i][1] * stride;
                    int lz = cz * stride + 1 + kCornerOffsets[i][2] * stride;
                    cornerVals[i] = density.at(lx, ly, lz);
                    if (cornerVals[i] < isovalue) {
                        cubeIndex |= static_cast<uint8_t>(1 << i);
                    }
                }

                if (kMCEdgeTable[cubeIndex] == 0)
                    continue;

                // Reset edge vertex cache
                for (int i = 0; i < 12; ++i)
                    edgeVerts[i] = UINT32_MAX;

                // Walk triangle list for this cube configuration
                for (int k = 0; kMCTriTable[cubeIndex][k] != -1; ++k) {
                    int edgeIdx = kMCTriTable[cubeIndex][k];

                    if (edgeVerts[edgeIdx] == UINT32_MAX) {
                        int c0 = kEdgeEndpoints[edgeIdx][0];
                        int c1 = kEdgeEndpoints[edgeIdx][1];
                        float v0 = cornerVals[c0];
                        float v1 = cornerVals[c1];

                        float t = (v1 != v0) ? (isovalue - v0) / (v1 - v0) : 0.5f;
                        t = std::clamp(t, 0.0f, 1.0f);

                        // Corner positions in cache coordinates
                        float p0x = static_cast<float>(cx * stride + 1 + kCornerOffsets[c0][0] * stride);
                        float p0y = static_cast<float>(cy * stride + 1 + kCornerOffsets[c0][1] * stride);
                        float p0z = static_cast<float>(cz * stride + 1 + kCornerOffsets[c0][2] * stride);
                        float p1x = static_cast<float>(cx * stride + 1 + kCornerOffsets[c1][0] * stride);
                        float p1y = static_cast<float>(cy * stride + 1 + kCornerOffsets[c1][1] * stride);
                        float p1z = static_cast<float>(cz * stride + 1 + kCornerOffsets[c1][2] * stride);

                        // Snap: if vertex is near either endpoint, snap to it
                        if (t < config_.snapEpsilon) {
                            t = 0.0f;
                        } else if (t > 1.0f - config_.snapEpsilon) {
                            t = 1.0f;
                        }

                        float posX = p0x + t * (p1x - p0x);
                        float posY = p0y + t * (p1y - p0y);
                        float posZ = p0z + t * (p1z - p0z);

                        // Chunk-local position (subtract the 1-voxel border offset)
                        float chunkX = posX - 1.0f;
                        float chunkY = posY - 1.0f;
                        float chunkZ = posZ - 1.0f;

                        // Clamp boundary vertices to exact chunk boundary positions.
                        // This ensures vertices at chunk edges align perfectly across chunks,
                        // eliminating floating-point precision differences that cause seams.
                        auto clampBoundary = [](float pos) -> float {
                            const float epsilon = 0.001f;
                            const float chunkEnd = static_cast<float>(kChunkSize);
                            if (pos >= -epsilon && pos <= epsilon) {
                                return 0.0f;
                            }
                            if (pos >= chunkEnd - epsilon && pos <= chunkEnd + epsilon) {
                                return chunkEnd;
                            }
                            return pos;
                        };

                        chunkX = clampBoundary(chunkX);
                        chunkY = clampBoundary(chunkY);
                        chunkZ = clampBoundary(chunkZ);

                        Vec3f normal = computeNormal(density, posX, posY, posZ);
                        uint16_t mat = material.sampleNearest(posX, posY, posZ);

                        SmoothVoxelVertex vert{};
                        vert.px = chunkX;
                        vert.py = chunkY;
                        vert.pz = chunkZ;
                        vert.nx = normal.x;
                        vert.ny = normal.y;
                        vert.nz = normal.z;
                        vert.material = SmoothVoxelVertex::packMaterial(mat);
                        vert.padding = 0;

                        edgeVerts[edgeIdx] = static_cast<uint32_t>(output.vertices.size());
                        output.vertices.push_back(vert);
                    }

                    output.indices.push_back(edgeVerts[edgeIdx]);
                }
            }
        }
    }

    // Merge vertices that snapped to the same grid point
    mergeCoincidentVertices(output, config_.snapEpsilon * static_cast<float>(stride) * 0.5f);

    return output;
}

} // namespace recurse

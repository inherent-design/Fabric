#include "recurse/world/SurfaceNetsMesher.hh"

#include "recurse/world/SnapMCTables.hh"

#include <cmath>
#include <unordered_map>

using fabric::K_CHUNK_SIZE;

namespace recurse {

namespace {

struct CellKey {
    int x, y, z;
    bool operator==(const CellKey&) const = default;
};

struct CellKeyHash {
    size_t operator()(const CellKey& k) const {
        auto h = std::hash<int>{};
        return h(k.x) ^ (h(k.y) * 2654435761u) ^ (h(k.z) * 40503u);
    }
};

} // namespace

SmoothChunkMeshData SurfaceNetsMesher::meshChunk(const ChunkDensityCache& density, const ChunkMaterialCache& material,
                                                 float isovalue, int lodLevel) {
    const int stride = 1 << lodLevel;
    const int gridSize = (K_CHUNK_SIZE / stride) + 1;

    SmoothChunkMeshData output;
    output.vertices.reserve(4096);
    output.indices.reserve(8192);

    std::unordered_map<CellKey, uint32_t, CellKeyHash> cellVertexMap;

    // Phase 1: identify active cells and place averaged vertices
    for (int cz = 0; cz < gridSize - 1; ++cz) {
        for (int cy = 0; cy < gridSize - 1; ++cy) {
            for (int cx = 0; cx < gridSize - 1; ++cx) {
                float corners[8];
                bool hasInside = false;
                bool hasOutside = false;

                for (int i = 0; i < 8; ++i) {
                    int lx = cx * stride + 1 + K_CORNER_OFFSETS[i][0] * stride;
                    int ly = cy * stride + 1 + K_CORNER_OFFSETS[i][1] * stride;
                    int lz = cz * stride + 1 + K_CORNER_OFFSETS[i][2] * stride;
                    corners[i] = density.at(lx, ly, lz);
                    if (corners[i] < isovalue)
                        hasInside = true;
                    else
                        hasOutside = true;
                }

                if (!hasInside || !hasOutside)
                    continue;

                // Average all edge-crossing points
                float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
                int crossCount = 0;

                for (int e = 0; e < 12; ++e) {
                    int c0 = K_EDGE_ENDPOINTS[e][0];
                    int c1 = K_EDGE_ENDPOINTS[e][1];
                    bool inside0 = corners[c0] < isovalue;
                    bool inside1 = corners[c1] < isovalue;

                    if (inside0 == inside1)
                        continue;

                    float v0 = corners[c0];
                    float v1 = corners[c1];
                    float t = (v1 != v0) ? (isovalue - v0) / (v1 - v0) : 0.5f;
                    t = std::clamp(t, 0.0f, 1.0f);

                    float p0x = static_cast<float>(cx * stride + 1 + K_CORNER_OFFSETS[c0][0] * stride);
                    float p0y = static_cast<float>(cy * stride + 1 + K_CORNER_OFFSETS[c0][1] * stride);
                    float p0z = static_cast<float>(cz * stride + 1 + K_CORNER_OFFSETS[c0][2] * stride);
                    float p1x = static_cast<float>(cx * stride + 1 + K_CORNER_OFFSETS[c1][0] * stride);
                    float p1y = static_cast<float>(cy * stride + 1 + K_CORNER_OFFSETS[c1][1] * stride);
                    float p1z = static_cast<float>(cz * stride + 1 + K_CORNER_OFFSETS[c1][2] * stride);

                    sumX += p0x + t * (p1x - p0x);
                    sumY += p0y + t * (p1y - p0y);
                    sumZ += p0z + t * (p1z - p0z);
                    ++crossCount;
                }

                if (crossCount == 0)
                    continue;

                float invCount = 1.0f / static_cast<float>(crossCount);
                float posX = sumX * invCount;
                float posY = sumY * invCount;
                float posZ = sumZ * invCount;

                float chunkX = posX - 1.0f;
                float chunkY = posY - 1.0f;
                float chunkZ = posZ - 1.0f;

                Vec3f normal = computeNormal(density, posX, posY, posZ);
                // Sample material from a solid corner rather than rounding
                // the averaged vertex position (which can land on the air side).
                uint16_t mat = 0;
                for (int i = 0; i < 8; ++i) {
                    if (corners[i] >= isovalue) {
                        int slx = cx * stride + 1 + K_CORNER_OFFSETS[i][0] * stride;
                        int sly = cy * stride + 1 + K_CORNER_OFFSETS[i][1] * stride;
                        int slz = cz * stride + 1 + K_CORNER_OFFSETS[i][2] * stride;
                        mat = material.at(slx, sly, slz);
                        break;
                    }
                }

                SmoothVoxelVertex vert{};
                vert.px = chunkX;
                vert.py = chunkY;
                vert.pz = chunkZ;
                vert.nx = normal.x;
                vert.ny = normal.y;
                vert.nz = normal.z;
                vert.material = SmoothVoxelVertex::packMaterial(mat);
                vert.padding = 0;

                cellVertexMap[{cx, cy, cz}] = static_cast<uint32_t>(output.vertices.size());
                output.vertices.push_back(vert);
            }
        }
    }

    // Phase 2: generate quads from shared primal edges
    // For each active cell, check 3 axis-aligned edges at the cell's "origin" corner.
    // Each edge is shared by 4 cells; we only emit when all 4 have dual vertices.
    for (int cz = 0; cz < gridSize - 1; ++cz) {
        for (int cy = 0; cy < gridSize - 1; ++cy) {
            for (int cx = 0; cx < gridSize - 1; ++cx) {
                // Corner 0 of this cell in cache coords
                int baseX = cx * stride + 1;
                int baseY = cy * stride + 1;
                int baseZ = cz * stride + 1;

                float val0 = density.at(baseX, baseY, baseZ);

                // X-axis edge: corner0 -> corner1
                // Shared by cells (cx,cy,cz), (cx,cy-1,cz), (cx,cy,cz-1), (cx,cy-1,cz-1)
                if (cy > 0 && cz > 0) {
                    float val1 = density.at(baseX + stride, baseY, baseZ);
                    if ((val0 < isovalue) != (val1 < isovalue)) {
                        auto a = cellVertexMap.find({cx, cy, cz});
                        auto b = cellVertexMap.find({cx, cy - 1, cz});
                        auto c = cellVertexMap.find({cx, cy - 1, cz - 1});
                        auto d = cellVertexMap.find({cx, cy, cz - 1});
                        if (a != cellVertexMap.end() && b != cellVertexMap.end() && c != cellVertexMap.end() &&
                            d != cellVertexMap.end()) {
                            uint32_t va = a->second, vb = b->second;
                            uint32_t vc = c->second, vd = d->second;
                            if (val0 < isovalue) {
                                output.indices.push_back(va);
                                output.indices.push_back(vb);
                                output.indices.push_back(vc);
                                output.indices.push_back(va);
                                output.indices.push_back(vc);
                                output.indices.push_back(vd);
                            } else {
                                output.indices.push_back(va);
                                output.indices.push_back(vc);
                                output.indices.push_back(vb);
                                output.indices.push_back(va);
                                output.indices.push_back(vd);
                                output.indices.push_back(vc);
                            }
                        }
                    }
                }

                // Y-axis edge: corner0 -> corner3
                // Shared by cells (cx,cy,cz), (cx-1,cy,cz), (cx,cy,cz-1), (cx-1,cy,cz-1)
                if (cx > 0 && cz > 0) {
                    float val3 = density.at(baseX, baseY + stride, baseZ);
                    if ((val0 < isovalue) != (val3 < isovalue)) {
                        auto a = cellVertexMap.find({cx, cy, cz});
                        auto b = cellVertexMap.find({cx - 1, cy, cz});
                        auto c = cellVertexMap.find({cx - 1, cy, cz - 1});
                        auto d = cellVertexMap.find({cx, cy, cz - 1});
                        if (a != cellVertexMap.end() && b != cellVertexMap.end() && c != cellVertexMap.end() &&
                            d != cellVertexMap.end()) {
                            uint32_t va = a->second, vb = b->second;
                            uint32_t vc = c->second, vd = d->second;
                            if (val0 < isovalue) {
                                output.indices.push_back(va);
                                output.indices.push_back(vd);
                                output.indices.push_back(vc);
                                output.indices.push_back(va);
                                output.indices.push_back(vc);
                                output.indices.push_back(vb);
                            } else {
                                output.indices.push_back(va);
                                output.indices.push_back(vc);
                                output.indices.push_back(vd);
                                output.indices.push_back(va);
                                output.indices.push_back(vb);
                                output.indices.push_back(vc);
                            }
                        }
                    }
                }

                // Z-axis edge: corner0 -> corner4
                // Shared by cells (cx,cy,cz), (cx-1,cy,cz), (cx,cy-1,cz), (cx-1,cy-1,cz)
                if (cx > 0 && cy > 0) {
                    float val4 = density.at(baseX, baseY, baseZ + stride);
                    if ((val0 < isovalue) != (val4 < isovalue)) {
                        auto a = cellVertexMap.find({cx, cy, cz});
                        auto b = cellVertexMap.find({cx - 1, cy, cz});
                        auto c = cellVertexMap.find({cx - 1, cy - 1, cz});
                        auto d = cellVertexMap.find({cx, cy - 1, cz});
                        if (a != cellVertexMap.end() && b != cellVertexMap.end() && c != cellVertexMap.end() &&
                            d != cellVertexMap.end()) {
                            uint32_t va = a->second, vb = b->second;
                            uint32_t vc = c->second, vd = d->second;
                            if (val0 < isovalue) {
                                output.indices.push_back(va);
                                output.indices.push_back(vb);
                                output.indices.push_back(vc);
                                output.indices.push_back(va);
                                output.indices.push_back(vc);
                                output.indices.push_back(vd);
                            } else {
                                output.indices.push_back(va);
                                output.indices.push_back(vc);
                                output.indices.push_back(vb);
                                output.indices.push_back(va);
                                output.indices.push_back(vd);
                                output.indices.push_back(vc);
                            }
                        }
                    }
                }
            }
        }
    }

    return output;
}

} // namespace recurse

#include "fabric/core/VoxelMesher.hh"

#include "fabric/core/EssencePalette.hh"

#include <algorithm>
#include <cmath>

namespace fabric {

namespace {

constexpr int kNeighborOff[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

struct FaceAxes {
    int normalAxis;
    int normalDir;
    int uAxis;
    int vAxis;
};

constexpr FaceAxes kFaceAxes[6] = {
    {0, 1, 2, 1},  // +X: slice on X, u=Z, v=Y
    {0, -1, 2, 1}, // -X
    {1, 1, 0, 2},  // +Y: slice on Y, u=X, v=Z
    {1, -1, 0, 2}, // -Y
    {2, 1, 0, 1},  // +Z: slice on Z, u=X, v=Y
    {2, -1, 0, 1}, // -Z
};

// Per-face vertex winding: 4 vertices as {use_u_max, use_v_max}
constexpr bool kVertUV[6][4][2] = {
    {{true, false}, {false, false}, {false, true}, {true, true}}, // +X
    {{false, false}, {true, false}, {true, true}, {false, true}}, // -X
    {{false, true}, {true, true}, {true, false}, {false, false}}, // +Y
    {{false, false}, {true, false}, {true, true}, {false, true}}, // -Y
    {{false, false}, {true, false}, {true, true}, {false, true}}, // +Z
    {{true, false}, {false, false}, {false, true}, {true, true}}, // -Z
};

} // namespace

bgfx::VertexLayout VoxelMesher::getVertexLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Uint8, false, false)
        .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Uint8, false, false)
        .end();
    return layout;
}

ChunkMeshData VoxelMesher::meshChunkData(int cx, int cy, int cz, const ChunkedGrid<float>& density,
                                         const ChunkedGrid<Vector4<float, Space::World>>& essence, float threshold,
                                         int lodLevel) {
    ChunkMeshData data;
    int base[3] = {cx * kChunkSize, cy * kChunkSize, cz * kChunkSize};
    const int stride = 1 << lodLevel;
    const int lodSize = kChunkSize / stride; // cells per axis at this LOD

    EssencePalette palette;

    // Sample density: max over stride-sized cell
    auto sampleDensity = [&](int bx, int by, int bz) -> float {
        float maxD = 0.0f;
        for (int dz = 0; dz < stride; ++dz)
            for (int dy = 0; dy < stride; ++dy)
                for (int dx = 0; dx < stride; ++dx)
                    maxD = std::max(maxD, density.get(bx + dx, by + dy, bz + dz));
        return maxD;
    };

    // Sample essence: majority-vote over stride-sized cell
    auto sampleEssence = [&](int bx, int by, int bz) -> Vector4<float, Space::World> {
        if (stride == 1)
            return essence.get(bx, by, bz);
        // Accumulate and average non-zero essences
        float sx = 0, sy = 0, sz = 0, sw = 0;
        int count = 0;
        for (int dz = 0; dz < stride; ++dz)
            for (int dy = 0; dy < stride; ++dy)
                for (int dx = 0; dx < stride; ++dx) {
                    auto e = essence.get(bx + dx, by + dy, bz + dz);
                    if (e.x != 0.0f || e.y != 0.0f || e.z != 0.0f || e.w != 0.0f) {
                        sx += e.x;
                        sy += e.y;
                        sz += e.z;
                        sw += e.w;
                        ++count;
                    }
                }
        if (count == 0)
            return Vector4<float, Space::World>(0.0f, 0.0f, 0.0f, 0.0f);
        return Vector4<float, Space::World>(sx / count, sy / count, sz / count, sw / count);
    };

    // Sample AO: max over stride-sized neighborhood
    auto sampleSolidLOD = [&](int wx, int wy, int wz) -> bool {
        for (int dz = 0; dz < stride; ++dz)
            for (int dy = 0; dy < stride; ++dy)
                for (int dx = 0; dx < stride; ++dx)
                    if (density.get(wx + dx, wy + dy, wz + dz) > threshold)
                        return true;
        return false;
    };

    for (int face = 0; face < 6; ++face) {
        const auto& ax = kFaceAxes[face];

        for (int slice = 0; slice < lodSize; ++slice) {
            bool mask[kChunkSize][kChunkSize] = {};
            uint16_t matIdx[kChunkSize][kChunkSize] = {};

            for (int v = 0; v < lodSize; ++v) {
                for (int u = 0; u < lodSize; ++u) {
                    int local[3];
                    local[ax.normalAxis] = slice * stride;
                    local[ax.uAxis] = u * stride;
                    local[ax.vAxis] = v * stride;

                    int wx = base[0] + local[0];
                    int wy = base[1] + local[1];
                    int wz = base[2] + local[2];

                    if (sampleDensity(wx, wy, wz) <= threshold)
                        continue;

                    // Neighbor check: sample the stride-sized cell adjacent in face direction
                    int nx = wx + kNeighborOff[face][0] * stride;
                    int ny = wy + kNeighborOff[face][1] * stride;
                    int nz = wz + kNeighborOff[face][2] * stride;

                    if (sampleDensity(nx, ny, nz) > threshold)
                        continue;

                    mask[u][v] = true;
                    auto e = sampleEssence(wx, wy, wz);
                    // Essence IS the material color (RGBA). Zero essence → default gray.
                    Vector4<float, Space::World> color;
                    if (e.x == 0.0f && e.y == 0.0f && e.z == 0.0f && e.w == 0.0f) {
                        color = Vector4<float, Space::World>(0.5f, 0.5f, 0.5f, 1.0f);
                    } else {
                        color = e;
                    }
                    matIdx[u][v] = palette.quantize(color);
                }
            }

            int normalLocal = slice * stride + (ax.normalDir > 0 ? stride : 0);
            int normalWorld = base[ax.normalAxis] + normalLocal;

            auto checkSolid = [&](int cu, int cv) -> int {
                int world[3];
                world[ax.normalAxis] = normalWorld;
                world[ax.uAxis] = base[ax.uAxis] + cu * stride;
                world[ax.vAxis] = base[ax.vAxis] + cv * stride;
                return sampleSolidLOD(world[0], world[1], world[2]) ? 1 : 0;
            };

            for (int v = 0; v < lodSize; ++v) {
                for (int u = 0; u < lodSize; ++u) {
                    if (!mask[u][v])
                        continue;

                    auto palIdx = matIdx[u][v];

                    int w = 1;
                    while (u + w < lodSize && mask[u + w][v] && matIdx[u + w][v] == palIdx)
                        ++w;

                    int h = 1;
                    while (v + h < lodSize) {
                        bool rowOk = true;
                        for (int du = 0; du < w; ++du) {
                            if (!mask[u + du][v + h] || matIdx[u + du][v + h] != palIdx) {
                                rowOk = false;
                                break;
                            }
                        }
                        if (!rowOk)
                            break;
                        ++h;
                    }

                    for (int dv = 0; dv < h; ++dv)
                        for (int du = 0; du < w; ++du)
                            mask[u + du][v + dv] = false;

                    int nd = slice * stride + (ax.normalDir > 0 ? stride : 0);
                    int u0 = u * stride;
                    int u1 = (u + w) * stride;
                    int v0 = v * stride;
                    int v1 = (v + h) * stride;

                    auto baseIdx = static_cast<uint32_t>(data.vertices.size());
                    uint8_t aoVals[4];

                    for (int vi = 0; vi < 4; ++vi) {
                        bool useUMax = kVertUV[face][vi][0];
                        bool useVMax = kVertUV[face][vi][1];

                        int refU = useUMax ? (u + w - 1) : u;
                        int refV = useVMax ? (v + h - 1) : v;
                        int su = useUMax ? 1 : -1;
                        int sv = useVMax ? 1 : -1;

                        int s1 = checkSolid(refU + su, refV);
                        int s2 = checkSolid(refU, refV + sv);
                        int c = checkSolid(refU + su, refV + sv);

                        aoVals[vi] = static_cast<uint8_t>((s1 && s2) ? 0 : 3 - (s1 + s2 + c));

                        uint8_t pos[3];
                        pos[ax.normalAxis] = static_cast<uint8_t>(nd);
                        pos[ax.uAxis] = static_cast<uint8_t>(useUMax ? u1 : u0);
                        pos[ax.vAxis] = static_cast<uint8_t>(useVMax ? v1 : v0);

                        data.vertices.push_back(
                            VoxelVertex::pack(pos[0], pos[1], pos[2], static_cast<uint8_t>(face), aoVals[vi], palIdx));
                    }

                    // Flip quad diagonal when AO anisotropy suggests it
                    if (aoVals[0] + aoVals[2] >= aoVals[1] + aoVals[3]) {
                        data.indices.push_back(baseIdx + 0);
                        data.indices.push_back(baseIdx + 1);
                        data.indices.push_back(baseIdx + 2);
                        data.indices.push_back(baseIdx + 0);
                        data.indices.push_back(baseIdx + 2);
                        data.indices.push_back(baseIdx + 3);
                    } else {
                        data.indices.push_back(baseIdx + 1);
                        data.indices.push_back(baseIdx + 2);
                        data.indices.push_back(baseIdx + 3);
                        data.indices.push_back(baseIdx + 1);
                        data.indices.push_back(baseIdx + 3);
                        data.indices.push_back(baseIdx + 0);
                    }
                }
            }
        }
    }

    // Export palette from EssencePalette to ChunkMeshData format
    data.palette.reserve(palette.paletteSize());
    for (size_t i = 0; i < palette.paletteSize(); ++i) {
        auto e = palette.lookup(static_cast<uint16_t>(i));
        data.palette.push_back({e.x, e.y, e.z, e.w});
    }

    return data;
}

ChunkMesh VoxelMesher::meshChunk(int cx, int cy, int cz, const ChunkedGrid<float>& density,
                                 const ChunkedGrid<Vector4<float, Space::World>>& essence, float threshold,
                                 int lodLevel) {
    auto data = meshChunkData(cx, cy, cz, density, essence, threshold, lodLevel);
    if (data.vertices.empty())
        return ChunkMesh{};

    ChunkMesh mesh;
    auto layout = getVertexLayout();

    mesh.vbh = bgfx::createVertexBuffer(
        bgfx::copy(data.vertices.data(), static_cast<uint32_t>(data.vertices.size() * sizeof(VoxelVertex))), layout);

    mesh.ibh = bgfx::createIndexBuffer(
        bgfx::copy(data.indices.data(), static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);

    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    mesh.palette = std::move(data.palette);
    mesh.valid = true;
    return mesh;
}

void VoxelMesher::destroyMesh(ChunkMesh& mesh) {
    if (bgfx::isValid(mesh.vbh)) {
        bgfx::destroy(mesh.vbh);
        mesh.vbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(mesh.ibh)) {
        bgfx::destroy(mesh.ibh);
        mesh.ibh = BGFX_INVALID_HANDLE;
    }
    mesh.indexCount = 0;
    mesh.palette.clear();
    mesh.valid = false;
}

// --- Water mesh generation ---

bgfx::VertexLayout VoxelMesher::getWaterVertexLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Uint8, false, false) // posNormalAO
        .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Uint8, false, false) // material
        .add(bgfx::Attrib::TexCoord2, 2, bgfx::AttribType::Uint8, false,
             false) // flowDx, flowDz (reinterpret as signed in shader)
        .end();
    return layout;
}

namespace {

constexpr float kMinWaterForMesh = 0.001f;

// Emit a quad (4 vertices, 6 indices) for a water face
void emitWaterQuad(WaterChunkMeshData& data, uint8_t x0, uint8_t y0, uint8_t z0, uint8_t x1, uint8_t y1, uint8_t z1,
                   uint8_t x2, uint8_t y2, uint8_t z2, uint8_t x3, uint8_t y3, uint8_t z3, uint8_t faceIdx,
                   int8_t flowDx, int8_t flowDz) {
    auto baseIdx = static_cast<uint32_t>(data.vertices.size());

    auto makeWV = [&](uint8_t px, uint8_t py, uint8_t pz) {
        WaterVertex wv;
        wv.base = VoxelVertex::pack(px, py, pz, faceIdx, 3, 0);
        wv.flowDx = flowDx;
        wv.flowDz = flowDz;
        return wv;
    };

    data.vertices.push_back(makeWV(x0, y0, z0));
    data.vertices.push_back(makeWV(x1, y1, z1));
    data.vertices.push_back(makeWV(x2, y2, z2));
    data.vertices.push_back(makeWV(x3, y3, z3));

    data.indices.push_back(baseIdx + 0);
    data.indices.push_back(baseIdx + 1);
    data.indices.push_back(baseIdx + 2);
    data.indices.push_back(baseIdx + 0);
    data.indices.push_back(baseIdx + 2);
    data.indices.push_back(baseIdx + 3);
}

int8_t clampFlow(float diff) {
    float scaled = diff * 127.0f;
    return static_cast<int8_t>(std::clamp(scaled, -127.0f, 127.0f));
}

} // namespace

WaterChunkMeshData VoxelMesher::meshWaterChunkData(int cx, int cy, int cz, const FieldLayer<float>& waterField,
                                                   const ChunkedGrid<float>& density, float solidThreshold) {
    WaterChunkMeshData data;
    int baseX = cx * kChunkSize;
    int baseY = cy * kChunkSize;
    int baseZ = cz * kChunkSize;

    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                int wx = baseX + lx;
                int wy = baseY + ly;
                int wz = baseZ + lz;

                float level = waterField.read(wx, wy, wz);
                if (level <= kMinWaterForMesh)
                    continue;

                // Skip if cell is solid
                if (density.get(wx, wy, wz) >= solidThreshold)
                    continue;

                // Compute flow direction from horizontal neighbor level differences
                float levelPx = waterField.read(wx + 1, wy, wz);
                float levelMx = waterField.read(wx - 1, wy, wz);
                float levelPz = waterField.read(wx, wy, wz + 1);
                float levelMz = waterField.read(wx, wy, wz - 1);

                // Flow goes from high to low: negative gradient
                float flowX = levelMx - levelPx;
                float flowZ = levelMz - levelPz;
                int8_t fdx = clampFlow(flowX);
                int8_t fdz = clampFlow(flowZ);

                // Local coords within chunk (uint8_t range: 0..31)
                auto px = static_cast<uint8_t>(lx);
                auto pz = static_cast<uint8_t>(lz);
                auto py0 = static_cast<uint8_t>(ly);

                // Top face Y: fractional based on water level
                // Water level 1.0 = full cell, 0.5 = half cell
                // We quantize to uint8_t: ly + level maps to sub-cell position
                // Since positions are in local chunk coords (0-32), top = ly + 1 when full
                // For fractional, we use the nearest uint8_t value
                uint8_t topY;
                if (level >= 1.0f) {
                    topY = static_cast<uint8_t>(ly + 1);
                } else {
                    // Scale level within the cell: ly + level
                    float fracY = static_cast<float>(ly) + level;
                    topY = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(fracY)), ly, ly + 1));
                }

                // Check each face for exposure
                // +X face (face 0): exposed if neighbor +X has no water at same level or is solid
                auto neighborExposed = [&](int nx, int ny, int nz) -> bool {
                    if (density.get(nx, ny, nz) >= solidThreshold)
                        return false; // solid neighbor blocks the face from view
                    float nLevel = waterField.read(nx, ny, nz);
                    if (nLevel <= kMinWaterForMesh)
                        return true; // no water neighbor = exposed
                    // Suppress face between cells with same water level
                    if (std::fabs(nLevel - level) < kMinWaterForMesh)
                        return false;
                    return true;
                };

                // +X face (face 0)
                if (neighborExposed(wx + 1, wy, wz)) {
                    emitWaterQuad(data, px + 1, py0, pz + 1, px + 1, py0, pz, px + 1, topY, pz, px + 1, topY, pz + 1, 0,
                                  fdx, fdz);
                }

                // -X face (face 1)
                if (neighborExposed(wx - 1, wy, wz)) {
                    emitWaterQuad(data, px, py0, pz, px, py0, pz + 1, px, topY, pz + 1, px, topY, pz, 1, fdx, fdz);
                }

                // +Y face (face 2) -- top face, always exposed unless water above
                {
                    float aboveLevel = waterField.read(wx, wy + 1, wz);
                    bool aboveSolid = density.get(wx, wy + 1, wz) >= solidThreshold;
                    if (!aboveSolid && aboveLevel <= kMinWaterForMesh) {
                        emitWaterQuad(data, px, topY, pz + 1, px + 1, topY, pz + 1, px + 1, topY, pz, px, topY, pz, 2,
                                      fdx, fdz);
                    }
                }

                // -Y face (face 3) -- bottom face
                {
                    float belowLevel = waterField.read(wx, wy - 1, wz);
                    bool belowSolid = density.get(wx, wy - 1, wz) >= solidThreshold;
                    if (!belowSolid && belowLevel <= kMinWaterForMesh) {
                        emitWaterQuad(data, px, py0, pz, px + 1, py0, pz, px + 1, py0, pz + 1, px, py0, pz + 1, 3, fdx,
                                      fdz);
                    }
                }

                // +Z face (face 4)
                if (neighborExposed(wx, wy, wz + 1)) {
                    emitWaterQuad(data, px, py0, pz + 1, px + 1, py0, pz + 1, px + 1, topY, pz + 1, px, topY, pz + 1, 4,
                                  fdx, fdz);
                }

                // -Z face (face 5)
                if (neighborExposed(wx, wy, wz - 1)) {
                    emitWaterQuad(data, px + 1, py0, pz, px, py0, pz, px, topY, pz, px + 1, topY, pz, 5, fdx, fdz);
                }
            }
        }
    }

    return data;
}

WaterChunkMesh VoxelMesher::meshWaterChunk(int cx, int cy, int cz, const FieldLayer<float>& waterField,
                                           const ChunkedGrid<float>& density, float solidThreshold) {
    auto data = meshWaterChunkData(cx, cy, cz, waterField, density, solidThreshold);
    if (data.vertices.empty())
        return WaterChunkMesh{};

    WaterChunkMesh mesh;
    auto layout = getWaterVertexLayout();

    mesh.vbh = bgfx::createVertexBuffer(
        bgfx::copy(data.vertices.data(), static_cast<uint32_t>(data.vertices.size() * sizeof(WaterVertex))), layout);

    mesh.ibh = bgfx::createIndexBuffer(
        bgfx::copy(data.indices.data(), static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);

    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    mesh.valid = true;
    return mesh;
}

void VoxelMesher::destroyWaterMesh(WaterChunkMesh& mesh) {
    if (bgfx::isValid(mesh.vbh)) {
        bgfx::destroy(mesh.vbh);
        mesh.vbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(mesh.ibh)) {
        bgfx::destroy(mesh.ibh);
        mesh.ibh = BGFX_INVALID_HANDLE;
    }
    mesh.indexCount = 0;
    mesh.valid = false;
}

} // namespace fabric

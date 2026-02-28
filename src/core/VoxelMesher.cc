#include "fabric/core/VoxelMesher.hh"

#include "fabric/core/EssencePalette.hh"

#include <algorithm>

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
                    float r, g, b, a;
                    if (e.x == 0.0f && e.y == 0.0f && e.z == 0.0f && e.w == 0.0f) {
                        r = 0.5f;
                        g = 0.5f;
                        b = 0.5f;
                        a = 1.0f;
                    } else {
                        r = e.y;
                        g = e.z;
                        b = e.x;
                        a = 1.0f - e.w * 0.5f;
                    }
                    matIdx[u][v] = palette.quantize(Vector4<float, Space::World>(r, g, b, a));
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

} // namespace fabric

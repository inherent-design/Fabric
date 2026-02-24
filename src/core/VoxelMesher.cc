#include "fabric/core/VoxelMesher.hh"

#include <unordered_map>

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

// Quantize RGBA floats to a uint32 key for palette deduplication
uint32_t colorKey(float r, float g, float b, float a) {
    auto toByte = [](float f) -> uint8_t {
        return static_cast<uint8_t>(f * 255.0f + 0.5f);
    };
    return static_cast<uint32_t>(toByte(r)) | (static_cast<uint32_t>(toByte(g)) << 8) |
           (static_cast<uint32_t>(toByte(b)) << 16) | (static_cast<uint32_t>(toByte(a)) << 24);
}

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
                                         const ChunkedGrid<Vector4<float, Space::World>>& essence, float threshold) {
    ChunkMeshData data;
    int base[3] = {cx * kChunkSize, cy * kChunkSize, cz * kChunkSize};

    std::unordered_map<uint32_t, uint16_t> paletteMap;
    auto getOrAddPalette = [&](float r, float g, float b, float a) -> uint16_t {
        uint32_t key = colorKey(r, g, b, a);
        auto it = paletteMap.find(key);
        if (it != paletteMap.end())
            return it->second;
        auto idx = static_cast<uint16_t>(data.palette.size());
        data.palette.push_back({r, g, b, a});
        paletteMap[key] = idx;
        return idx;
    };

    for (int face = 0; face < 6; ++face) {
        const auto& ax = kFaceAxes[face];

        for (int slice = 0; slice < kChunkSize; ++slice) {
            bool mask[kChunkSize][kChunkSize] = {};
            uint16_t matIdx[kChunkSize][kChunkSize] = {};

            for (int v = 0; v < kChunkSize; ++v) {
                for (int u = 0; u < kChunkSize; ++u) {
                    int local[3];
                    local[ax.normalAxis] = slice;
                    local[ax.uAxis] = u;
                    local[ax.vAxis] = v;

                    int wx = base[0] + local[0];
                    int wy = base[1] + local[1];
                    int wz = base[2] + local[2];

                    if (density.get(wx, wy, wz) <= threshold)
                        continue;

                    int nx = wx + kNeighborOff[face][0];
                    int ny = wy + kNeighborOff[face][1];
                    int nz = wz + kNeighborOff[face][2];

                    if (density.get(nx, ny, nz) > threshold)
                        continue;

                    mask[u][v] = true;
                    auto e = essence.get(wx, wy, wz);
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
                    matIdx[u][v] = getOrAddPalette(r, g, b, a);
                }
            }

            int normalWorld = base[ax.normalAxis] + slice + ax.normalDir;

            auto checkSolid = [&](int cu, int cv) -> int {
                int world[3];
                world[ax.normalAxis] = normalWorld;
                world[ax.uAxis] = base[ax.uAxis] + cu;
                world[ax.vAxis] = base[ax.vAxis] + cv;
                return density.get(world[0], world[1], world[2]) > threshold ? 1 : 0;
            };

            for (int v = 0; v < kChunkSize; ++v) {
                for (int u = 0; u < kChunkSize; ++u) {
                    if (!mask[u][v])
                        continue;

                    auto palIdx = matIdx[u][v];

                    int w = 1;
                    while (u + w < kChunkSize && mask[u + w][v] && matIdx[u + w][v] == palIdx)
                        ++w;

                    int h = 1;
                    while (v + h < kChunkSize) {
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

                    int nd = slice + (ax.normalDir > 0 ? 1 : 0);
                    int u0 = u;
                    int u1 = u + w;
                    int v0 = v;
                    int v1 = v + h;

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

    return data;
}

ChunkMesh VoxelMesher::meshChunk(int cx, int cy, int cz, const ChunkedGrid<float>& density,
                                 const ChunkedGrid<Vector4<float, Space::World>>& essence, float threshold) {
    auto data = meshChunkData(cx, cy, cz, density, essence, threshold);
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

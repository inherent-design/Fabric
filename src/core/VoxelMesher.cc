#include "fabric/core/VoxelMesher.hh"

namespace fabric {

namespace {

// Face directions: +X, -X, +Y, -Y, +Z, -Z
constexpr float kNormals[6][3] = {
    {1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f},
};

// Neighbor offsets matching the face order
constexpr int kNeighborOff[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

// 4 vertices per face, CCW winding when viewed from outside the cube.
// Each vertex is an offset from the cell origin (x, y, z).
constexpr float kFaceVerts[6][4][3] = {
    // +X face (normal +X)
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
    // -X face (normal -X)
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
    // +Y face (normal +Y)
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
    // -Y face (normal -Y)
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
    // +Z face (normal +Z)
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
    // -Z face (normal -Z)
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
};

} // namespace

bgfx::VertexLayout VoxelMesher::getVertexLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
        .end();
    return layout;
}

ChunkMeshData VoxelMesher::meshChunkData(int cx, int cy, int cz, const ChunkedGrid<float>& density,
                                         const ChunkedGrid<Vector4<float, Space::World>>& essence, float threshold) {
    ChunkMeshData data;

    int baseX = cx * kChunkSize;
    int baseY = cy * kChunkSize;
    int baseZ = cz * kChunkSize;

    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                int wx = baseX + lx;
                int wy = baseY + ly;
                int wz = baseZ + lz;

                float d = density.get(wx, wy, wz);
                if (d <= threshold)
                    continue;

                // Read essence for color
                auto e = essence.get(wx, wy, wz);
                float cr, cg, cb, ca;
                if (e.x == 0.0f && e.y == 0.0f && e.z == 0.0f && e.w == 0.0f) {
                    // Default gray for zero essence
                    cr = 0.5f;
                    cg = 0.5f;
                    cb = 0.5f;
                    ca = 1.0f;
                } else {
                    // essence = [Order, Chaos, Life, Decay]
                    // Color: R = Chaos, G = Life, B = Order, A = 1 - Decay*0.5
                    cr = e.y;               // Chaos
                    cg = e.z;               // Life
                    cb = e.x;               // Order
                    ca = 1.0f - e.w * 0.5f; // Decay
                }

                for (int face = 0; face < 6; ++face) {
                    int nx = wx + kNeighborOff[face][0];
                    int ny = wy + kNeighborOff[face][1];
                    int nz = wz + kNeighborOff[face][2];

                    float nd = density.get(nx, ny, nz);
                    if (nd > threshold)
                        continue;

                    // Emit quad for this face
                    auto base = static_cast<uint32_t>(data.vertices.size());

                    for (int v = 0; v < 4; ++v) {
                        VoxelVertex vert;
                        vert.px = static_cast<float>(wx) + kFaceVerts[face][v][0];
                        vert.py = static_cast<float>(wy) + kFaceVerts[face][v][1];
                        vert.pz = static_cast<float>(wz) + kFaceVerts[face][v][2];
                        vert.nx = kNormals[face][0];
                        vert.ny = kNormals[face][1];
                        vert.nz = kNormals[face][2];
                        vert.r = cr;
                        vert.g = cg;
                        vert.b = cb;
                        vert.a = ca;
                        data.vertices.push_back(vert);
                    }

                    // Two triangles: (0,1,2) and (0,2,3)
                    data.indices.push_back(base + 0);
                    data.indices.push_back(base + 1);
                    data.indices.push_back(base + 2);
                    data.indices.push_back(base + 0);
                    data.indices.push_back(base + 2);
                    data.indices.push_back(base + 3);
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
    mesh.valid = false;
}

} // namespace fabric

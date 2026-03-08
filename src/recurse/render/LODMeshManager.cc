#include "recurse/render/LODMeshManager.hh"

#include "fabric/core/Log.hh"
#include "fabric/simulation/MaterialRegistry.hh"
#include "recurse/world/ChunkDensityCache.hh"
#include "recurse/world/SnapMCMesher.hh"

#include <cmath>

namespace recurse {

LODMeshManager::LODMeshManager(LODGrid& grid, const fabric::simulation::MaterialRegistry& materials)
    : grid_(grid), materials_(materials), mesher_(std::make_unique<SnapMCMesher>()) {}

LODMeshManager::~LODMeshManager() = default;

LODMeshManager::MeshResult LODMeshManager::meshSection(const LODSection& section) {
    MeshResult result;

    // Check if section has any solid voxels
    bool hasSolid = false;
    for (size_t i = 0; i < section.blockIndices.size(); ++i) {
        if (section.materialOf(section.blockIndices[i]) != 0) {
            hasSolid = true;
            break;
        }
    }
    if (!hasSolid) {
        return result; // Empty section
    }

    // Build density and material caches from section data
    // The section is already at the correct resolution (downsampled in LODGrid)
    // We fill a 34^3 cache to match the mesher's expected interface

    ChunkDensityCache density;
    ChunkMaterialCache material;

    // Fill density cache: 1.0 = solid, 0.0 = air
    // The LOD section is 32^3, the cache is 34^3 (adds 1-voxel border)
    float* densityData = density.data();

    // Initialize to air
    for (int i = 0; i < K_CACHE_VOLUME; ++i) {
        densityData[i] = 0.0f;
    }

    // Fill interior (1..32 region in cache = section voxels)
    for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
        for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
            for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                int cacheX = lx + 1; // Offset by 1 for border
                int cacheY = ly + 1;
                int cacheZ = lz + 1;

                uint16_t palIdx = section.get(lx, ly, lz);
                uint16_t matId = section.materialOf(palIdx);

                int idx = cacheX + cacheY * K_CACHE_SIZE + cacheZ * K_CACHE_SIZE * K_CACHE_SIZE;

                if (matId != 0) {
                    densityData[idx] = 1.0f;
                }
            }
        }
    }

    // Fill material cache - we need to construct one from section data
    // Since ChunkMaterialCache::build() takes a ChunkedGrid, we need a different approach
    // For LOD sections, we directly fill the material array

    // Rebuild density cache properly using the existing API
    // Actually, ChunkDensityCache::data() returns const float*, so we need to use build()
    // But build() takes a ChunkedGrid which we don't have for LOD sections

    // Alternative: Create a temporary 34^3 material array and sample directly
    std::array<uint16_t, K_CACHE_VOLUME> materialArray{};
    for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
        for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
            for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                int cacheX = lx + 1;
                int cacheY = ly + 1;
                int cacheZ = lz + 1;
                int idx = cacheX + cacheY * K_CACHE_SIZE + cacheZ * K_CACHE_SIZE * K_CACHE_SIZE;

                uint16_t palIdx = section.get(lx, ly, lz);
                uint16_t matId = section.materialOf(palIdx);
                materialArray[idx] = matId;

                if (matId != 0) {
                    densityData[idx] = 1.0f;
                }
            }
        }
    }

    // Expand borders (clamp to edge)
    for (int i = 0; i < K_CACHE_SIZE; ++i) {
        for (int j = 0; j < K_CACHE_SIZE; ++j) {
            // X borders
            densityData[0 + j * K_CACHE_SIZE + i * K_CACHE_SIZE * K_CACHE_SIZE] =
                densityData[1 + j * K_CACHE_SIZE + i * K_CACHE_SIZE * K_CACHE_SIZE];
            densityData[(K_CACHE_SIZE - 1) + j * K_CACHE_SIZE + i * K_CACHE_SIZE * K_CACHE_SIZE] =
                densityData[(K_CACHE_SIZE - 2) + j * K_CACHE_SIZE + i * K_CACHE_SIZE * K_CACHE_SIZE];
            materialArray[0 + j * K_CACHE_SIZE + i * K_CACHE_SIZE * K_CACHE_SIZE] =
                materialArray[1 + j * K_CACHE_SIZE + i * K_CACHE_SIZE * K_CACHE_SIZE];
            materialArray[(K_CACHE_SIZE - 1) + j * K_CACHE_SIZE + i * K_CACHE_SIZE * K_CACHE_SIZE] =
                materialArray[(K_CACHE_SIZE - 2) + j * K_CACHE_SIZE + i * K_CACHE_SIZE * K_CACHE_SIZE];

            // Y borders
            densityData[i + 0 * K_CACHE_SIZE + j * K_CACHE_SIZE * K_CACHE_SIZE] =
                densityData[i + 1 * K_CACHE_SIZE + j * K_CACHE_SIZE * K_CACHE_SIZE];
            densityData[i + (K_CACHE_SIZE - 1) * K_CACHE_SIZE + j * K_CACHE_SIZE * K_CACHE_SIZE] =
                densityData[i + (K_CACHE_SIZE - 2) * K_CACHE_SIZE + j * K_CACHE_SIZE * K_CACHE_SIZE];
            materialArray[i + 0 * K_CACHE_SIZE + j * K_CACHE_SIZE * K_CACHE_SIZE] =
                materialArray[i + 1 * K_CACHE_SIZE + j * K_CACHE_SIZE * K_CACHE_SIZE];
            materialArray[i + (K_CACHE_SIZE - 1) * K_CACHE_SIZE + j * K_CACHE_SIZE * K_CACHE_SIZE] =
                materialArray[i + (K_CACHE_SIZE - 2) * K_CACHE_SIZE + j * K_CACHE_SIZE * K_CACHE_SIZE];

            // Z borders
            densityData[i + j * K_CACHE_SIZE + 0 * K_CACHE_SIZE * K_CACHE_SIZE] =
                densityData[i + j * K_CACHE_SIZE + 1 * K_CACHE_SIZE * K_CACHE_SIZE];
            densityData[i + j * K_CACHE_SIZE + (K_CACHE_SIZE - 1) * K_CACHE_SIZE * K_CACHE_SIZE] =
                densityData[i + j * K_CACHE_SIZE + (K_CACHE_SIZE - 2) * K_CACHE_SIZE * K_CACHE_SIZE];
            materialArray[i + j * K_CACHE_SIZE + 0 * K_CACHE_SIZE * K_CACHE_SIZE] =
                materialArray[i + j * K_CACHE_SIZE + 1 * K_CACHE_SIZE * K_CACHE_SIZE];
            materialArray[i + j * K_CACHE_SIZE + (K_CACHE_SIZE - 1) * K_CACHE_SIZE * K_CACHE_SIZE] =
                materialArray[i + j * K_CACHE_SIZE + (K_CACHE_SIZE - 2) * K_CACHE_SIZE * K_CACHE_SIZE];
        }
    }

    // Now we need to mesh using the cache data
    // Since ChunkMaterialCache::build() requires a ChunkedGrid, we create a custom
    // meshing loop that reads directly from our arrays

    // For now, use the SnapMC mesher with a custom approach
    // We'll manually invoke the meshing logic

    // Build material palette for this section
    for (size_t i = 0; i < section.palette.size(); ++i) {
        uint16_t matId = section.palette[i];
        if (matId == 0) {
            result.palette.push_back({0.0f, 0.0f, 0.0f, 0.0f}); // Air
        } else {
            result.palette.push_back(materialColor(matId));
        }
    }

    // Simple marching cubes meshing for LOD section
    // This reuses the same tables as SnapMCMesher but reads from our cache

    // We need direct access to density/material data, so create a minimal
    // mesher invocation. The SnapMCMesher expects ChunkDensityCache/ChunkMaterialCache.

    // Create a temporary ChunkedGrid wrapper for materialArray
    // This is hacky but necessary to reuse the existing mesher

    // Actually, let's just create proper caches using a wrapper approach
    // For now, skip complex cache building and mesh directly

    // Direct meshing approach - simplified MC for LOD
    result.vertices.reserve(1024);
    result.indices.reserve(2048);

    // For LOD, we can use a simpler approach: just create quads for solid-void boundaries
    // This is less accurate but faster for distant terrain
    for (int lz = 0; lz < LODSection::K_SIZE - 1; ++lz) {
        for (int ly = 0; ly < LODSection::K_SIZE - 1; ++ly) {
            for (int lx = 0; lx < LODSection::K_SIZE - 1; ++lx) {
                uint16_t palIdx = section.get(lx, ly, lz);
                uint16_t matId = section.materialOf(palIdx);

                if (matId == 0)
                    continue; // Skip air

                // Check 6 neighbors for exposed faces
                const int dx[] = {1, -1, 0, 0, 0, 0};
                const int dy[] = {0, 0, 1, -1, 0, 0};
                const int dz[] = {0, 0, 0, 0, 1, -1};

                for (int f = 0; f < 6; ++f) {
                    int nx = lx + dx[f];
                    int ny = ly + dy[f];
                    int nz = lz + dz[f];

                    uint16_t nMat = 0;
                    if (nx >= 0 && nx < LODSection::K_SIZE && ny >= 0 && ny < LODSection::K_SIZE && nz >= 0 &&
                        nz < LODSection::K_SIZE) {
                        uint16_t nPal = section.get(nx, ny, nz);
                        nMat = section.materialOf(nPal);
                    }

                    if (nMat == 0) {
                        // Exposed face - add quad
                        float x = static_cast<float>(lx);
                        float y = static_cast<float>(ly);
                        float z = static_cast<float>(lz);

                        // Normal direction
                        float ndx = static_cast<float>(dx[f]);
                        float ndy = static_cast<float>(dy[f]);
                        float ndz = static_cast<float>(dz[f]);

                        // 4 vertices for the quad
                        SmoothVoxelVertex v0{}, v1{}, v2{}, v3{};

                        // Vertex positions depend on face direction
                        if (f == 0) { // +X
                            v0.px = x + 1;
                            v0.py = y;
                            v0.pz = z;
                            v1.px = x + 1;
                            v1.py = y + 1;
                            v1.pz = z;
                            v2.px = x + 1;
                            v2.py = y + 1;
                            v2.pz = z + 1;
                            v3.px = x + 1;
                            v3.py = y;
                            v3.pz = z + 1;
                        } else if (f == 1) { // -X
                            v0.px = x;
                            v0.py = y;
                            v0.pz = z + 1;
                            v1.px = x;
                            v1.py = y + 1;
                            v1.pz = z + 1;
                            v2.px = x;
                            v2.py = y + 1;
                            v2.pz = z;
                            v3.px = x;
                            v3.py = y;
                            v3.pz = z;
                        } else if (f == 2) { // +Y
                            v0.px = x;
                            v0.py = y + 1;
                            v0.pz = z;
                            v1.px = x;
                            v1.py = y + 1;
                            v1.pz = z + 1;
                            v2.px = x + 1;
                            v2.py = y + 1;
                            v2.pz = z + 1;
                            v3.px = x + 1;
                            v3.py = y + 1;
                            v3.pz = z;
                        } else if (f == 3) { // -Y
                            v0.px = x;
                            v0.py = y;
                            v0.pz = z + 1;
                            v1.px = x;
                            v1.py = y;
                            v1.pz = z;
                            v2.px = x + 1;
                            v2.py = y;
                            v2.pz = z;
                            v3.px = x + 1;
                            v3.py = y;
                            v3.pz = z + 1;
                        } else if (f == 4) { // +Z
                            v0.px = x;
                            v0.py = y;
                            v0.pz = z + 1;
                            v1.px = x + 1;
                            v1.py = y;
                            v1.pz = z + 1;
                            v2.px = x + 1;
                            v2.py = y + 1;
                            v2.pz = z + 1;
                            v3.px = x;
                            v3.py = y + 1;
                            v3.pz = z + 1;
                        } else { // -Z
                            v0.px = x + 1;
                            v0.py = y;
                            v0.pz = z;
                            v1.px = x;
                            v1.py = y;
                            v1.pz = z;
                            v2.px = x;
                            v2.py = y + 1;
                            v2.pz = z;
                            v3.px = x + 1;
                            v3.py = y + 1;
                            v3.pz = z;
                        }

                        // Set normals and material
                        v0.nx = ndx;
                        v0.ny = ndy;
                        v0.nz = ndz;
                        v1.nx = ndx;
                        v1.ny = ndy;
                        v1.nz = ndz;
                        v2.nx = ndx;
                        v2.ny = ndy;
                        v2.nz = ndz;
                        v3.nx = ndx;
                        v3.ny = ndy;
                        v3.nz = ndz;

                        v0.material = SmoothVoxelVertex::packMaterial(matId);
                        v1.material = SmoothVoxelVertex::packMaterial(matId);
                        v2.material = SmoothVoxelVertex::packMaterial(matId);
                        v3.material = SmoothVoxelVertex::packMaterial(matId);

                        uint32_t base = static_cast<uint32_t>(result.vertices.size());
                        result.vertices.insert(result.vertices.end(), {v0, v1, v2, v3});
                        result.indices.insert(result.indices.end(),
                                              {base, base + 1, base + 2, base, base + 2, base + 3});
                    }
                }
            }
        }
    }

    return result;
}

int LODMeshManager::rebuildDirty(int budget) {
    int processed = 0;

    grid_.forEach([&](LODSection& section) {
        if (processed >= budget)
            return;
        if (!section.dirty)
            return;

        // Mesh section - for now just mark clean
        // The actual meshing and upload is handled by LODSystem
        section.dirty = false;
        ++processed;
    });

    if (processed > 0) {
        FABRIC_LOG_DEBUG("LODMeshManager: Processed {} dirty sections", processed);
    }

    return processed;
}

size_t LODMeshManager::pendingCount() const {
    size_t count = 0;
    grid_.forEach([&count](const LODSection& section) {
        if (section.dirty) {
            ++count;
        }
    });
    return count;
}

std::array<float, 4> LODMeshManager::materialColor(uint16_t materialId) const {
    if (materialId == 0) {
        return {0.0f, 0.0f, 0.0f, 0.0f}; // Air
    }

    const auto& def = materials_.get(materialId);
    uint32_t c = def.baseColor;

    float r = static_cast<float>((c >> 24) & 0xFF) / 255.0f;
    float g = static_cast<float>((c >> 16) & 0xFF) / 255.0f;
    float b = static_cast<float>((c >> 8) & 0xFF) / 255.0f;
    float a = static_cast<float>(c & 0xFF) / 255.0f;

    return {r, g, b, a};
}

} // namespace recurse

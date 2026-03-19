#include "recurse/render/LODMeshManager.hh"

#include "fabric/log/Log.hh"
#include "recurse/simulation/MaterialRegistry.hh"

#include <algorithm>

namespace recurse {

namespace {

constexpr uint8_t K_LOD_SHADER_AO = 3u;

uint8_t clampPackedCoord(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, LODSection::K_SIZE));
}

uint8_t lodFaceNormalIndex(int face) {
    switch (face) {
        case 0:
            return 0u;
        case 1:
            return 1u;
        case 2:
            return 2u;
        case 3:
            return 3u;
        case 4:
            return 4u;
        default:
            return 5u;
    }
}

void appendPackedQuad(LODMeshManager::MeshResult& result, int face, int x, int y, int z, uint16_t paletteIndex) {
    const uint32_t base = static_cast<uint32_t>(result.vertices.size());
    const uint8_t normalIdx = lodFaceNormalIndex(face);

    auto push = [&](int px, int py, int pz) {
        result.vertices.push_back(VoxelVertex::pack(clampPackedCoord(px), clampPackedCoord(py), clampPackedCoord(pz),
                                                    normalIdx, K_LOD_SHADER_AO, paletteIndex));
    };

    switch (face) {
        case 0: // +X
            push(x + 1, y, z);
            push(x + 1, y + 1, z);
            push(x + 1, y + 1, z + 1);
            push(x + 1, y, z + 1);
            break;
        case 1: // -X
            push(x, y, z + 1);
            push(x, y + 1, z + 1);
            push(x, y + 1, z);
            push(x, y, z);
            break;
        case 2: // +Y
            push(x, y + 1, z);
            push(x, y + 1, z + 1);
            push(x + 1, y + 1, z + 1);
            push(x + 1, y + 1, z);
            break;
        case 3: // -Y
            push(x, y, z + 1);
            push(x, y, z);
            push(x + 1, y, z);
            push(x + 1, y, z + 1);
            break;
        case 4: // +Z
            push(x, y, z + 1);
            push(x + 1, y, z + 1);
            push(x + 1, y + 1, z + 1);
            push(x, y + 1, z + 1);
            break;
        default: // -Z
            push(x + 1, y, z);
            push(x, y, z);
            push(x, y + 1, z);
            push(x + 1, y + 1, z);
            break;
    }

    result.indices.insert(result.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // namespace

LODMeshManager::LODMeshManager(LODGrid& grid, const recurse::simulation::MaterialRegistry& materials)
    : grid_(grid), materials_(materials) {}

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

    // Terrain appearance contract: distant LOD uses the same MaterialDef::baseColor
    // truth as full-res chunk meshes.
    for (size_t i = 0; i < section.palette.size(); ++i) {
        uint16_t matId = section.palette[i];
        if (matId == 0) {
            result.palette.push_back({0.0f, 0.0f, 0.0f, 0.0f}); // Air
        } else {
            result.palette.push_back(materials_.terrainAppearanceColor(matId));
        }
    }

    // Keep LOD generation simple: emit packed quads for solid-void boundaries and
    // let the render transform apply section scale.
    result.vertices.reserve(1024);
    result.indices.reserve(2048);

    constexpr int dx[] = {1, -1, 0, 0, 0, 0};
    constexpr int dy[] = {0, 0, 1, -1, 0, 0};
    constexpr int dz[] = {0, 0, 0, 0, 1, -1};

    for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
        for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
            for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                uint16_t palIdx = section.get(lx, ly, lz);
                uint16_t matId = section.materialOf(palIdx);

                if (matId == 0)
                    continue; // Skip air

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
                        appendPackedQuad(result, f, lx, ly, lz, palIdx);
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

} // namespace recurse

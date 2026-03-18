#include "recurse/render/LODGrid.hh"

#include "fabric/log/Log.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <algorithm>
#include <array>

namespace recurse {

namespace {

constexpr int K_TOP_LAYER_WEIGHT = 3;

struct MaterialTally {
    uint16_t materialId = simulation::material_ids::AIR;
    int weightedScore = 0;
    int sampleCount = 0;
};

int materialSemanticPriority(uint16_t materialId) {
    switch (materialId) {
        case simulation::material_ids::SAND:
        case simulation::material_ids::GRAVEL:
            return 4;
        case simulation::material_ids::WATER:
            return 3;
        case simulation::material_ids::DIRT:
            return 2;
        case simulation::material_ids::STONE:
            return 1;
        default:
            return 0;
    }
}

void accumulateMaterialTally(std::array<MaterialTally, 8>& tallies, int& tallyCount, uint16_t materialId, int weight) {
    for (int i = 0; i < tallyCount; ++i) {
        if (tallies[static_cast<size_t>(i)].materialId == materialId) {
            tallies[static_cast<size_t>(i)].weightedScore += weight;
            ++tallies[static_cast<size_t>(i)].sampleCount;
            return;
        }
    }

    auto& tally = tallies[static_cast<size_t>(tallyCount++)];
    tally.materialId = materialId;
    tally.weightedScore = weight;
    tally.sampleCount = 1;
}

uint16_t reduceMaterialGroup(const LODSection& child, int clx, int cly, int clz) {
    std::array<MaterialTally, 8> tallies{};
    int tallyCount = 0;
    int airCount = 0;

    for (int dz = 0; dz <= 1; ++dz) {
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = 0; dx <= 1; ++dx) {
                uint16_t palIdx = child.get(clx + dx, cly + dy, clz + dz);
                uint16_t matId = child.materialOf(palIdx);
                if (matId == simulation::material_ids::AIR) {
                    ++airCount;
                    continue;
                }

                const int weight = (dy == 1) ? K_TOP_LAYER_WEIGHT : 1;
                accumulateMaterialTally(tallies, tallyCount, matId, weight);
            }
        }
    }

    if (airCount > 4) {
        return simulation::material_ids::AIR;
    }

    uint16_t bestMat = simulation::material_ids::AIR;
    int bestScore = -1;
    int bestCount = -1;
    int bestPriority = -1;
    for (int i = 0; i < tallyCount; ++i) {
        const auto& tally = tallies[static_cast<size_t>(i)];
        const int priority = materialSemanticPriority(tally.materialId);
        const bool isBetter =
            tally.weightedScore > bestScore || (tally.weightedScore == bestScore && tally.sampleCount > bestCount) ||
            (tally.weightedScore == bestScore && tally.sampleCount == bestCount && priority > bestPriority) ||
            (tally.weightedScore == bestScore && tally.sampleCount == bestCount && priority == bestPriority &&
             tally.materialId < bestMat);
        if (!isBetter) {
            continue;
        }

        bestMat = tally.materialId;
        bestScore = tally.weightedScore;
        bestCount = tally.sampleCount;
        bestPriority = priority;
    }

    return bestMat;
}

} // namespace

// --- LODGrid ---

LODSection* LODGrid::get(LODSectionKey key) {
    auto it = sections_.find(key.value);
    if (it != sections_.end()) {
        return it->second.get();
    }
    return nullptr;
}

LODSection* LODGrid::getOrCreate(int level, int sx, int sy, int sz) {
    auto key = LODSectionKey::make(level, sx, sy, sz);
    auto it = sections_.find(key.value);
    if (it != sections_.end()) {
        return it->second.get();
    }

    auto section = std::make_unique<LODSection>();
    section->level = level;
    section->origin = sectionOrigin(level, sx, sy, sz);
    section->blockIndices.assign(LODSection::K_VOLUME, 0);
    section->palette.clear();
    section->palette.push_back(simulation::material_ids::AIR); // Index 0 = air
    section->dirty = true;

    auto* ptr = section.get();
    sections_[key.value] = std::move(section);
    FABRIC_LOG_TRACE("LODGrid: Created section level={} ({},{},{})", level, sx, sy, sz);
    return ptr;
}

void LODGrid::tryBuildParent(int childLevel, int cx, int cy, int cz) {
    if (childLevel >= 15) {
        return; // Max LOD level
    }

    int parentLevel = childLevel + 1;
    int px = cx >> 1;
    int py = cy >> 1;
    int pz = cz >> 1;

    // Check if all 8 children exist
    if (!hasAllChildren(childLevel, px * 2, py * 2, pz * 2)) {
        return;
    }

    // Collect all 8 children
    std::array<LODSection*, 8> children{};
    int idx = 0;
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = 0; dx <= 1; ++dx) {
                auto key = LODSectionKey::make(childLevel, px * 2 + dx, py * 2 + dy, pz * 2 + dz);
                children[idx++] = get(key);
            }
        }
    }

    // Create or get parent
    auto* parent = getOrCreate(parentLevel, px, py, pz);

    // Downsample children into parent
    downsample(*parent, children);

    FABRIC_LOG_TRACE("LODGrid: Built parent level={} ({},{},{}) from 8 children", parentLevel, px, py, pz);

    // Recursively try to build grandparent
    tryBuildParent(parentLevel, px, py, pz);
}

void LODGrid::downsample(LODSection& parent, const std::array<LODSection*, 8>& children) {
    parent.palette.clear();
    parent.palette.push_back(simulation::material_ids::AIR); // Index 0 = air
    parent.blockIndices.assign(LODSection::K_VOLUME, 0);

    // Map from materialId to palette index
    auto getOrCreatePalIdx = [&parent](uint16_t matId) -> uint16_t {
        for (size_t i = 0; i < parent.palette.size(); ++i) {
            if (parent.palette[i] == matId) {
                return static_cast<uint16_t>(i);
            }
        }
        if (parent.palette.size() >= 65535) {
            return 0; // Palette full, use air
        }
        parent.palette.push_back(matId);
        return static_cast<uint16_t>(parent.palette.size() - 1);
    };

    // For each voxel in the parent, sample from the corresponding 2x2x2 group
    // in the matching child section.
    for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
        for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
            for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                // Which child section? (based on high bit of local coord)
                int childIdx = ((lx >> 4) & 1) | (((ly >> 4) & 1) << 1) | (((lz >> 4) & 1) << 2);
                auto* child = children[childIdx];
                if (!child) {
                    parent.set(lx, ly, lz, 0);
                    continue;
                }

                // Sample 2x2x2 group within the child
                int clx = (lx & 0xF) * 2;
                int cly = (ly & 0xF) * 2;
                int clz = (lz & 0xF) * 2;

                const uint16_t bestMat = reduceMaterialGroup(*child, clx, cly, clz);

                uint16_t palIdx = getOrCreatePalIdx(bestMat);
                parent.set(lx, ly, lz, palIdx);
            }
        }
    }

    parent.dirty = true;
}

void LODGrid::remove(LODSectionKey key) {
    auto it = sections_.find(key.value);
    if (it != sections_.end()) {
        sections_.erase(it);
        FABRIC_LOG_TRACE("LODGrid: Removed section level={} ({},{},{})", key.level(), key.x(), key.y(), key.z());
    }
}

bool LODGrid::hasAllChildren(int level, int sx, int sy, int sz) {
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = 0; dx <= 1; ++dx) {
                auto key = LODSectionKey::make(level, sx + dx, sy + dy, sz + dz);
                if (!get(key)) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace recurse

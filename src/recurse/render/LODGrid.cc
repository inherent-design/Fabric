#include "recurse/render/LODGrid.hh"

#include "fabric/core/Log.hh"
#include <algorithm>
#include <array>

namespace recurse {

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
    section->origin = Vec3i(sx * kSectionWorldSize, sy * kSectionWorldSize, sz * kSectionWorldSize);
    section->blockIndices.assign(LODSection::kVolume, 0);
    section->palette.clear();
    section->palette.push_back(0); // Index 0 = air (materialId 1)
    section->dirty = true;

    auto* ptr = section.get();
    sections_[key.value] = std::move(section);
    FABRIC_LOG_DEBUG("LODGrid: Created section level={} ({},{},{})", level, sx, sy, sz);
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

    FABRIC_LOG_DEBUG("LODGrid: Built parent level={} ({},{},{}) from 8 children", parentLevel, px, py, pz);

    // Recursively try to build grandparent
    tryBuildParent(parentLevel, px, py, pz);
}

void LODGrid::downsample(LODSection& parent, const std::array<LODSection*, 8>& children) {
    parent.palette.clear();
    parent.palette.push_back(1); // Index 0 = air (materialId 1)

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

    // For each voxel in the parent (16^3 at half resolution),
    // sample from 2x2x2 group in children
    for (int lz = 0; lz < LODSection::kSize / 2; ++lz) {
        for (int ly = 0; ly < LODSection::kSize / 2; ++ly) {
            for (int lx = 0; lx < LODSection::kSize / 2; ++lx) {
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

                // Count materials in 2x2x2 group
                uint16_t materialCounts[256] = {};
                int airCount = 0;

                for (int dz = 0; dz <= 1; ++dz) {
                    for (int dy = 0; dy <= 1; ++dy) {
                        for (int dx = 0; dx <= 1; ++dx) {
                            uint16_t palIdx = child->get(clx + dx, cly + dy, clz + dz);
                            uint16_t matId = child->materialOf(palIdx);
                            if (matId == 1) {
                                ++airCount;
                            } else {
                                ++materialCounts[matId % 256];
                            }
                        }
                    }
                }

                // If >50% air, result is air (preserves silhouette)
                if (airCount > 4) {
                    parent.set(lx, ly, lz, 0);
                    continue;
                }

                // Most-common non-air material wins
                uint16_t bestMat = 1;
                int bestCount = 0;
                for (int m = 1; m < 256; ++m) {
                    if (materialCounts[m] > bestCount) {
                        bestCount = materialCounts[m];
                        bestMat = static_cast<uint16_t>(m);
                    }
                }

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
        FABRIC_LOG_DEBUG("LODGrid: Removed section level={} ({},{},{})", key.level(), key.x(), key.y(), key.z());
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

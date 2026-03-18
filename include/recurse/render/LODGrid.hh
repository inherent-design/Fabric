#pragma once

#include "fabric/render/Geometry.hh"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace recurse {

using Vec3i = fabric::Vector3<int, fabric::Space::World>;
using fabric::Vec3f;

// 64-bit packed section key: [level:4][sx:20][sy:20][sz:20]
struct LODSectionKey {
    uint64_t value = 0;

    static LODSectionKey make(int level, int32_t sx, int32_t sy, int32_t sz) {
        LODSectionKey k;
        k.value = (static_cast<uint64_t>(level & 0xF) << 60) | (static_cast<uint64_t>(sx & 0xFFFFF) << 40) |
                  (static_cast<uint64_t>(sy & 0xFFFFF) << 20) | (static_cast<uint64_t>(sz & 0xFFFFF));
        return k;
    }

    int level() const { return static_cast<int>((value >> 60) & 0xF); }
    int32_t x() const { return static_cast<int32_t>((value >> 40) & 0xFFFFF); }
    int32_t y() const { return static_cast<int32_t>((value >> 20) & 0xFFFFF); }
    int32_t z() const { return static_cast<int32_t>(value & 0xFFFFF); }

    bool operator==(const LODSectionKey& other) const { return value == other.value; }
};

// 32^3 section at a specific LOD level
struct LODSection {
    static constexpr int K_SIZE = 32;
    static constexpr int K_VOLUME = K_SIZE * K_SIZE * K_SIZE;

    int level = 0;
    Vec3i origin{0, 0, 0};              // World-space origin in LOD0 coords
    std::vector<uint16_t> blockIndices; // K_VOLUME entries, palette-indexed
    std::vector<uint16_t> palette;      // materialId list (index 0 = air)
    bool dirty = true;

    void set(int lx, int ly, int lz, uint16_t palIdx) {
        if (blockIndices.size() != K_VOLUME) {
            blockIndices.assign(K_VOLUME, 0);
        }
        blockIndices[lx + ly * K_SIZE + lz * K_SIZE * K_SIZE] = palIdx;
    }

    uint16_t get(int lx, int ly, int lz) const {
        if (blockIndices.empty())
            return 0;
        return blockIndices[lx + ly * K_SIZE + lz * K_SIZE * K_SIZE];
    }

    uint16_t materialOf(uint16_t palIdx) const {
        if (palIdx >= palette.size())
            return 0;
        return palette[palIdx];
    }
};

class LODGrid {
  public:
    static constexpr int K_SECTION_WORLD_SIZE = LODSection::K_SIZE;

    static constexpr int sectionScale(int level) { return 1 << level; }
    static constexpr int sectionWorldSize(int level) { return K_SECTION_WORLD_SIZE * sectionScale(level); }
    static Vec3i sectionOrigin(int level, int sx, int sy, int sz) {
        const int worldSize = sectionWorldSize(level);
        return Vec3i(sx * worldSize, sy * worldSize, sz * worldSize);
    }
    static constexpr int sectionCoordFromOrigin(int level, int originComponent) {
        return originComponent / sectionWorldSize(level);
    }
    static LODSectionKey keyForSection(const LODSection& section) {
        return LODSectionKey::make(section.level, sectionCoordFromOrigin(section.level, section.origin.x),
                                   sectionCoordFromOrigin(section.level, section.origin.y),
                                   sectionCoordFromOrigin(section.level, section.origin.z));
    }

    LODSection* get(LODSectionKey key);
    LODSection* getOrCreate(int level, int sx, int sy, int sz);
    void tryBuildParent(int childLevel, int cx, int cy, int cz);
    void downsample(LODSection& parent, const std::array<LODSection*, 8>& children);
    void remove(LODSectionKey key);
    void clear() { sections_.clear(); }
    size_t sectionCount() const { return sections_.size(); }

    template <typename Fn> void forEach(Fn&& fn) {
        for (auto& [key, section] : sections_) {
            fn(*section);
        }
    }

    template <typename Fn> void forEach(Fn&& fn) const {
        for (const auto& [key, section] : sections_) {
            fn(*section);
        }
    }

  private:
    std::unordered_map<uint64_t, std::unique_ptr<LODSection>> sections_;

    static uint64_t packKey(int level, int sx, int sy, int sz) { return LODSectionKey::make(level, sx, sy, sz).value; }

    bool hasAllChildren(int level, int sx, int sy, int sz);
};

} // namespace recurse

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace fabric {

inline constexpr int kChunkSize = 32;
inline constexpr int kChunkShift = 5;
inline constexpr int kChunkMask = kChunkSize - 1;
inline constexpr int kChunkVolume = kChunkSize * kChunkSize * kChunkSize;

template <typename T> class ChunkedGrid {
  public:
    // C++20 arithmetic right shift gives floor division for power-of-2
    static void worldToChunk(int wx, int wy, int wz, int& cx, int& cy, int& cz, int& lx, int& ly, int& lz) {
        cx = wx >> kChunkShift;
        cy = wy >> kChunkShift;
        cz = wz >> kChunkShift;
        lx = wx & kChunkMask;
        ly = wy & kChunkMask;
        lz = wz & kChunkMask;
    }

    T get(int x, int y, int z) const {
        int cx, cy, cz, lx, ly, lz;
        worldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
        auto key = packKey(cx, cy, cz);
        auto it = chunks_.find(key);
        if (it == chunks_.end())
            return T{};
        return (*it->second)[localIndex(lx, ly, lz)];
    }

    void set(int x, int y, int z, const T& value) {
        int cx, cy, cz, lx, ly, lz;
        worldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
        auto key = packKey(cx, cy, cz);
        auto& chunk = chunks_[key];
        if (!chunk) {
            chunk = std::make_unique<std::array<T, kChunkVolume>>();
            chunk->fill(T{});
        }
        (*chunk)[localIndex(lx, ly, lz)] = value;
    }

    bool hasChunk(int cx, int cy, int cz) const { return chunks_.contains(packKey(cx, cy, cz)); }

    void removeChunk(int cx, int cy, int cz) { chunks_.erase(packKey(cx, cy, cz)); }

    size_t chunkCount() const { return chunks_.size(); }

    std::vector<std::tuple<int, int, int>> activeChunks() const {
        std::vector<std::tuple<int, int, int>> result;
        result.reserve(chunks_.size());
        for (const auto& [key, _] : chunks_) {
            auto [cx, cy, cz] = unpackKey(key);
            result.emplace_back(cx, cy, cz);
        }
        return result;
    }

    void forEachCell(int cx, int cy, int cz, std::function<void(int, int, int, T&)> fn) {
        auto key = packKey(cx, cy, cz);
        auto it = chunks_.find(key);
        if (it == chunks_.end())
            return;
        auto& data = *it->second;
        int baseX = cx * kChunkSize;
        int baseY = cy * kChunkSize;
        int baseZ = cz * kChunkSize;
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int ly = 0; ly < kChunkSize; ++ly) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    fn(baseX + lx, baseY + ly, baseZ + lz, data[localIndex(lx, ly, lz)]);
                }
            }
        }
    }

    // Returns: [+x, -x, +y, -y, +z, -z]
    std::array<T, 6> getNeighbors6(int x, int y, int z) const {
        return {{get(x + 1, y, z), get(x - 1, y, z), get(x, y + 1, z), get(x, y - 1, z), get(x, y, z + 1),
                 get(x, y, z - 1)}};
    }

  private:
    std::unordered_map<int64_t, std::unique_ptr<std::array<T, kChunkVolume>>> chunks_;

    static int64_t packKey(int cx, int cy, int cz) {
        return (static_cast<int64_t>(cx) << 42) | (static_cast<int64_t>(cy & 0x1FFFFF) << 21) |
               static_cast<int64_t>(cz & 0x1FFFFF);
    }

    static std::tuple<int, int, int> unpackKey(int64_t key) {
        int cx = static_cast<int>(key >> 42);
        int cy = static_cast<int>((key >> 21) & 0x1FFFFF);
        int cz = static_cast<int>(key & 0x1FFFFF);
        // Sign-extend 21-bit values
        if (cy & 0x100000)
            cy |= ~0x1FFFFF;
        if (cz & 0x100000)
            cz |= ~0x1FFFFF;
        return {cx, cy, cz};
    }

    static int localIndex(int lx, int ly, int lz) { return lx + ly * kChunkSize + lz * kChunkSize * kChunkSize; }
};

} // namespace fabric

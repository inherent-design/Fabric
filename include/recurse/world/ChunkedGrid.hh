#pragma once

#include "recurse/world/ChunkCoordUtils.hh"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace recurse {

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
        auto key = packChunkKey(cx, cy, cz);
        auto it = chunks_.find(key);
        if (it == chunks_.end())
            return T{};
        return (*it->second)[localIndex(lx, ly, lz)];
    }

    void set(int x, int y, int z, const T& value) {
        int cx, cy, cz, lx, ly, lz;
        worldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
        auto key = packChunkKey(cx, cy, cz);
        auto& chunk = chunks_[key];
        if (!chunk) {
            chunk = std::make_unique<std::array<T, kChunkVolume>>();
            chunk->fill(T{});
        }
        (*chunk)[localIndex(lx, ly, lz)] = value;
    }

    bool hasChunk(int cx, int cy, int cz) const { return chunks_.contains(packChunkKey(cx, cy, cz)); }

    void removeChunk(int cx, int cy, int cz) { chunks_.erase(packChunkKey(cx, cy, cz)); }

    size_t chunkCount() const { return chunks_.size(); }

    std::vector<std::tuple<int, int, int>> activeChunks() const {
        std::vector<std::tuple<int, int, int>> result;
        result.reserve(chunks_.size());
        for (const auto& [key, _] : chunks_) {
            auto [cx, cy, cz] = unpackChunkKey(key);
            result.emplace_back(cx, cy, cz);
        }
        return result;
    }

    void forEachCell(int cx, int cy, int cz, std::function<void(int, int, int, T&)> fn) {
        auto key = packChunkKey(cx, cy, cz);
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

    void forEachCell(int cx, int cy, int cz, std::function<void(int, int, int, const T&)> fn) const {
        auto key = packChunkKey(cx, cy, cz);
        auto it = chunks_.find(key);
        if (it == chunks_.end())
            return;
        const auto& data = *it->second;
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

    void forEachChunk(std::function<void(int, int, int)> fn) const {
        for (const auto& [key, _] : chunks_) {
            auto [cx, cy, cz] = unpackChunkKey(key);
            fn(cx, cy, cz);
        }
    }

    void clear() { chunks_.clear(); }

    // Returns: [+x, -x, +y, -y, +z, -z]
    std::array<T, 6> getNeighbors6(int x, int y, int z) const {
        return {{get(x + 1, y, z), get(x - 1, y, z), get(x, y + 1, z), get(x, y - 1, z), get(x, y, z + 1),
                 get(x, y, z - 1)}};
    }

    /// Trilinear interpolation at arbitrary world-space position.
    /// Returns interpolated value from 8 surrounding integer grid points.
    /// Requires T to support arithmetic operations (float, double).
    T sampleLinear(float wx, float wy, float wz) const {
        int x0 = static_cast<int>(std::floor(wx));
        int y0 = static_cast<int>(std::floor(wy));
        int z0 = static_cast<int>(std::floor(wz));

        float fx = wx - static_cast<float>(x0);
        float fy = wy - static_cast<float>(y0);
        float fz = wz - static_cast<float>(z0);

        T c000 = get(x0, y0, z0);
        T c100 = get(x0 + 1, y0, z0);
        T c010 = get(x0, y0 + 1, z0);
        T c110 = get(x0 + 1, y0 + 1, z0);
        T c001 = get(x0, y0, z0 + 1);
        T c101 = get(x0 + 1, y0, z0 + 1);
        T c011 = get(x0, y0 + 1, z0 + 1);
        T c111 = get(x0 + 1, y0 + 1, z0 + 1);

        T c00 = c000 * (1.0f - fx) + c100 * fx;
        T c10 = c010 * (1.0f - fx) + c110 * fx;
        T c01 = c001 * (1.0f - fx) + c101 * fx;
        T c11 = c011 * (1.0f - fx) + c111 * fx;

        T c0 = c00 * (1.0f - fy) + c10 * fy;
        T c1 = c01 * (1.0f - fy) + c11 * fy;

        return c0 * (1.0f - fz) + c1 * fz;
    }

  private:
    std::map<int64_t, std::unique_ptr<std::array<T, kChunkVolume>>> chunks_;

    static std::tuple<int, int, int> unpackChunkKey(int64_t key) {
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

} // namespace recurse

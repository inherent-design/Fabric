#pragma once

#include "fabric/world/ChunkCoordUtils.hh"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace fabric {

namespace detail {
constexpr int log2Floor(int v) {
    int r = 0;
    while (v >>= 1)
        ++r;
    return r;
}
} // namespace detail

/// Sparse infinite grid partitioned into fixed-size chunks.
///
/// ChunkSize is already part of the public contract. Current Recurse callers
/// mostly use the default size of 32, but alternative chunk sizes remain part
/// of the engine-facing template boundary for future multi-project use.
template <typename T, int ChunkSize = 32> class ChunkedGrid {
  public:
    static_assert(ChunkSize > 0 && (ChunkSize & (ChunkSize - 1)) == 0, "ChunkSize must be a power of 2");

    static constexpr int K_SIZE = ChunkSize;
    static constexpr int K_SHIFT = detail::log2Floor(ChunkSize);
    static constexpr int K_MASK = ChunkSize - 1;
    static constexpr int K_VOLUME = ChunkSize * ChunkSize * ChunkSize;

    static void worldToChunk(int wx, int wy, int wz, int& cx, int& cy, int& cz, int& lx, int& ly, int& lz) {
        cx = wx >> K_SHIFT;
        cy = wy >> K_SHIFT;
        cz = wz >> K_SHIFT;
        lx = wx & K_MASK;
        ly = wy & K_MASK;
        lz = wz & K_MASK;
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
            chunk = std::make_unique<std::array<T, K_VOLUME>>();
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
        int baseX = cx * K_SIZE;
        int baseY = cy * K_SIZE;
        int baseZ = cz * K_SIZE;
        for (int lz = 0; lz < K_SIZE; ++lz) {
            for (int ly = 0; ly < K_SIZE; ++ly) {
                for (int lx = 0; lx < K_SIZE; ++lx) {
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
        int baseX = cx * K_SIZE;
        int baseY = cy * K_SIZE;
        int baseZ = cz * K_SIZE;
        for (int lz = 0; lz < K_SIZE; ++lz) {
            for (int ly = 0; ly < K_SIZE; ++ly) {
                for (int lx = 0; lx < K_SIZE; ++lx) {
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

    /// Return the 6 face-adjacent neighbor values around a world-space cell.
    ///
    /// The return order is {+x, -x, +y, -y, +z, -z}.
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
    std::map<int64_t, std::unique_ptr<std::array<T, K_VOLUME>>> chunks_;

    static int localIndex(int lx, int ly, int lz) { return lx + ly * K_SIZE + lz * K_SIZE * K_SIZE; }
};

} // namespace fabric

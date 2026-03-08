#include "fabric/simulation/FallingSandSystem.hh"
#include <algorithm>

namespace fabric::simulation {

namespace {
// Directional-alternating sweep: reverses x/z iteration each frame to prevent
// systematic bias in cellular automata. cellFn returns true if it moved a cell.
template <typename Fn> bool sweepChunk(uint64_t frameIndex, Fn&& cellFn) {
    bool anyChange = false;
    bool reverseDir = (frameIndex & 1) != 0;

    for (int ly = 0; ly < kChunkSize; ++ly) {
        int lxStart = reverseDir ? kChunkSize - 1 : 0;
        int lxEnd = reverseDir ? -1 : kChunkSize;
        int lxStep = reverseDir ? -1 : 1;

        int lzStart = reverseDir ? kChunkSize - 1 : 0;
        int lzEnd = reverseDir ? -1 : kChunkSize;
        int lzStep = reverseDir ? -1 : 1;

        for (int lz = lzStart; lz != lzEnd; lz += lzStep) {
            for (int lx = lxStart; lx != lxEnd; lx += lxStep) {
                if (cellFn(lx, ly, lz))
                    anyChange = true;
            }
        }
    }
    return anyChange;
}
} // namespace

FallingSandSystem::FallingSandSystem(const MaterialRegistry& registry) : registry_(registry) {}

VoxelCell FallingSandSystem::readCell(ChunkPos pos, int lx, int ly, int lz, const SimulationGrid& grid,
                                      const GhostCellManager& ghosts) const {
    if (lx >= 0 && lx < kChunkSize && ly >= 0 && ly < kChunkSize && lz >= 0 && lz < kChunkSize) {
        int wx = pos.x * kChunkSize + lx;
        int wy = pos.y * kChunkSize + ly;
        int wz = pos.z * kChunkSize + lz;
        // Immediate-mode: read from write buffer so earlier writes in the
        // same sweep are visible (prevents cell collisions/losses).
        return grid.readFromWriteBuffer(wx, wy, wz);
    }
    return ghosts.readGhost(pos, lx, ly, lz);
}

bool FallingSandSystem::canDisplace(VoxelCell mover, VoxelCell target) const {
    if (target.materialId == material_ids::AIR)
        return true;
    const auto& targetDef = registry_.get(target.materialId);
    if (targetDef.moveType == MoveType::Static)
        return false;
    const auto& moverDef = registry_.get(mover.materialId);
    return moverDef.density > targetDef.density;
}

void FallingSandSystem::writeSwap(ChunkPos pos, int srcLx, int srcLy, int srcLz, int dstLx, int dstLy, int dstLz,
                                  VoxelCell srcCell, VoxelCell dstCell, SimulationGrid& grid,
                                  ChunkActivityTracker& tracker) const {

    // Source is always in-bounds (we only move cells we own)
    int srcWx = pos.x * kChunkSize + srcLx;
    int srcWy = pos.y * kChunkSize + srcLy;
    int srcWz = pos.z * kChunkSize + srcLz;
    grid.writeCell(srcWx, srcWy, srcWz, dstCell);

    // Destination may be out of chunk bounds
    if (dstLx >= 0 && dstLx < kChunkSize && dstLy >= 0 && dstLy < kChunkSize && dstLz >= 0 && dstLz < kChunkSize) {
        int dstWx = pos.x * kChunkSize + dstLx;
        int dstWy = pos.y * kChunkSize + dstLy;
        int dstWz = pos.z * kChunkSize + dstLz;
        grid.writeCell(dstWx, dstWy, dstWz, srcCell);
    } else {
        // Cross-chunk write: compute world coords from chunk + local offset
        int dstWx = pos.x * kChunkSize + dstLx;
        int dstWy = pos.y * kChunkSize + dstLy;
        int dstWz = pos.z * kChunkSize + dstLz;
        grid.writeCell(dstWx, dstWy, dstWz, srcCell);

        // Wake the neighbor chunk
        int ncx = dstWx >> 5;
        int ncy = dstWy >> 5;
        int ncz = dstWz >> 5;
        // Handle negative coordinates: arithmetic right shift gives floor division
        // for power-of-2 on two's complement (C++20 guarantees this)
        tracker.notifyBoundaryChange(ChunkPos{ncx, ncy, ncz});
    }
}

bool FallingSandSystem::simulateGravity(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                                        ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng) {

    return sweepChunk(frameIndex, [&](int lx, int ly, int lz) -> bool {
        VoxelCell cell = readCell(pos, lx, ly, lz, grid, ghosts);
        if (cell.materialId == material_ids::AIR)
            return false;

        const auto& def = registry_.get(cell.materialId);
        if (def.moveType == MoveType::Static || def.moveType == MoveType::Liquid)
            return false;

        // Try direct fall (down = ly-1)
        VoxelCell below = readCell(pos, lx, ly - 1, lz, grid, ghosts);
        if (canDisplace(cell, below)) {
            writeSwap(pos, lx, ly, lz, lx, ly - 1, lz, cell, below, grid, tracker);
            return true;
        }

        // Powder diagonal cascade
        if (def.moveType == MoveType::Powder) {
            struct Offset {
                int dx, dy, dz;
            };
            std::array<Offset, 4> diags = {{{-1, -1, 0}, {1, -1, 0}, {0, -1, -1}, {0, -1, 1}}};
            std::shuffle(diags.begin(), diags.end(), rng);

            for (const auto& [dx, dy, dz] : diags) {
                VoxelCell target = readCell(pos, lx + dx, ly + dy, lz + dz, grid, ghosts);
                if (canDisplace(cell, target)) {
                    writeSwap(pos, lx, ly, lz, lx + dx, ly + dy, lz + dz, cell, target, grid, tracker);
                    return true;
                }
            }
        }
        return false;
    });
}

bool FallingSandSystem::simulateLiquid(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                                       ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng) {

    return sweepChunk(frameIndex, [&](int lx, int ly, int lz) -> bool {
        VoxelCell cell = readCell(pos, lx, ly, lz, grid, ghosts);
        const auto& def = registry_.get(cell.materialId);
        if (def.moveType != MoveType::Liquid)
            return false;

        // 1. Gravity (same as powder)
        VoxelCell below = readCell(pos, lx, ly - 1, lz, grid, ghosts);
        if (canDisplace(cell, below)) {
            writeSwap(pos, lx, ly, lz, lx, ly - 1, lz, cell, below, grid, tracker);
            return true;
        }

        // 2. Diagonal-down
        struct Offset {
            int dx, dy, dz;
        };
        std::array<Offset, 4> diags = {{{-1, -1, 0}, {1, -1, 0}, {0, -1, -1}, {0, -1, 1}}};
        std::shuffle(diags.begin(), diags.end(), rng);

        for (const auto& [dx, dy, dz] : diags) {
            VoxelCell target = readCell(pos, lx + dx, ly + dy, lz + dz, grid, ghosts);
            if (canDisplace(cell, target)) {
                writeSwap(pos, lx, ly, lz, lx + dx, ly + dy, lz + dz, cell, target, grid, tracker);
                return true;
            }
        }

        // 3. Horizontal flow (1 cell/tick)
        std::array<Offset, 4> horiz = {{{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
        std::shuffle(horiz.begin(), horiz.end(), rng);

        for (const auto& [dx, dy, dz] : horiz) {
            VoxelCell target = readCell(pos, lx + dx, ly + dy, lz + dz, grid, ghosts);
            if (target.materialId == material_ids::AIR) {
                writeSwap(pos, lx, ly, lz, lx + dx, ly + dy, lz + dz, cell, target, grid, tracker);
                return true;
            }
        }
        return false;
    });
}

void FallingSandSystem::simulateChunk(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                                      ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng) {
    bool gravityChanged = simulateGravity(pos, grid, ghosts, tracker, frameIndex, rng);
    bool liquidChanged = simulateLiquid(pos, grid, ghosts, tracker, frameIndex, rng);
    if (!gravityChanged && !liquidChanged)
        tracker.putToSleep(pos);
}

} // namespace fabric::simulation

#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include <algorithm>
#include <bit>

namespace recurse::simulation {

namespace {
// Directional-alternating sweep: reverses x/z iteration each frame to prevent
// systematic bias in cellular automata. cellFn returns true if it moved a cell.
template <typename Fn> bool sweepChunk(uint64_t frameIndex, Fn&& cellFn) {
    bool anyChange = false;
    bool reverseDir = (frameIndex & 1) != 0;

    for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
        int lxStart = reverseDir ? K_CHUNK_SIZE - 1 : 0;
        int lxEnd = reverseDir ? -1 : K_CHUNK_SIZE;
        int lxStep = reverseDir ? -1 : 1;

        int lzStart = reverseDir ? K_CHUNK_SIZE - 1 : 0;
        int lzEnd = reverseDir ? -1 : K_CHUNK_SIZE;
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

VoxelCell FallingSandSystem::readCell(ChunkCoord pos, int lx, int ly, int lz, const SimulationGrid& grid,
                                      const GhostCellManager& ghosts) const {
    if (lx >= 0 && lx < K_CHUNK_SIZE && ly >= 0 && ly < K_CHUNK_SIZE && lz >= 0 && lz < K_CHUNK_SIZE) {
        int wx = pos.x * K_CHUNK_SIZE + lx;
        int wy = pos.y * K_CHUNK_SIZE + ly;
        int wz = pos.z * K_CHUNK_SIZE + lz;
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

void FallingSandSystem::writeSwap(ChunkCoord pos, int srcLx, int srcLy, int srcLz, int dstLx, int dstLy, int dstLz,
                                  VoxelCell srcCell, VoxelCell dstCell, SimulationGrid& grid,
                                  ChunkActivityTracker& tracker, BoundaryWriteQueue& boundaryWrites,
                                  std::vector<CellSwap>& cellSwaps) const {

    // Source is always in-bounds (we only move cells we own)
    int srcWx = pos.x * K_CHUNK_SIZE + srcLx;
    int srcWy = pos.y * K_CHUNK_SIZE + srcLy;
    int srcWz = pos.z * K_CHUNK_SIZE + srcLz;
    grid.writeCell(srcWx, srcWy, srcWz, dstCell);

    // Record source-side change (src gets dstCell as new value)
    cellSwaps.push_back(
        CellSwap{pos, srcLx, srcLy, srcLz, std::bit_cast<uint32_t>(srcCell), std::bit_cast<uint32_t>(dstCell)});

    // Destination may be out of chunk bounds
    if (dstLx >= 0 && dstLx < K_CHUNK_SIZE && dstLy >= 0 && dstLy < K_CHUNK_SIZE && dstLz >= 0 &&
        dstLz < K_CHUNK_SIZE) {
        int dstWx = pos.x * K_CHUNK_SIZE + dstLx;
        int dstWy = pos.y * K_CHUNK_SIZE + dstLy;
        int dstWz = pos.z * K_CHUNK_SIZE + dstLz;
        grid.writeCell(dstWx, dstWy, dstWz, srcCell);

        // Record dest-side change (dst gets srcCell as new value)
        cellSwaps.push_back(
            CellSwap{pos, dstLx, dstLy, dstLz, std::bit_cast<uint32_t>(dstCell), std::bit_cast<uint32_t>(srcCell)});
    } else {
        // Cross-chunk write: defer to boundary queue (Phase 3b).
        // Dest-side CellSwap deferred; BoundaryWrite lacks old-cell data.
        int dstWx = pos.x * K_CHUNK_SIZE + dstLx;
        int dstWy = pos.y * K_CHUNK_SIZE + dstLy;
        int dstWz = pos.z * K_CHUNK_SIZE + dstLz;
        int ncx = dstWx >> 5;
        int ncy = dstWy >> 5;
        int ncz = dstWz >> 5;
        boundaryWrites.push_back(
            BoundaryWrite{dstWx, dstWy, dstWz, srcCell, srcWx, srcWy, srcWz, srcCell, ChunkCoord{ncx, ncy, ncz}});
    }
}

bool FallingSandSystem::simulateGravity(ChunkCoord pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                                        ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng,
                                        BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps) {

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
            writeSwap(pos, lx, ly, lz, lx, ly - 1, lz, cell, below, grid, tracker, boundaryWrites, cellSwaps);
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
                    writeSwap(pos, lx, ly, lz, lx + dx, ly + dy, lz + dz, cell, target, grid, tracker, boundaryWrites,
                              cellSwaps);
                    return true;
                }
            }
        }
        return false;
    });
}

bool FallingSandSystem::simulateLiquid(ChunkCoord pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                                       ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng,
                                       BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps) {

    return sweepChunk(frameIndex, [&](int lx, int ly, int lz) -> bool {
        VoxelCell cell = readCell(pos, lx, ly, lz, grid, ghosts);
        const auto& def = registry_.get(cell.materialId);
        if (def.moveType != MoveType::Liquid)
            return false;

        // 1. Gravity (same as powder)
        VoxelCell below = readCell(pos, lx, ly - 1, lz, grid, ghosts);
        if (canDisplace(cell, below)) {
            writeSwap(pos, lx, ly, lz, lx, ly - 1, lz, cell, below, grid, tracker, boundaryWrites, cellSwaps);
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
                writeSwap(pos, lx, ly, lz, lx + dx, ly + dy, lz + dz, cell, target, grid, tracker, boundaryWrites,
                          cellSwaps);
                return true;
            }
        }

        // 3. Horizontal flow (1 cell/tick)
        std::array<Offset, 4> horiz = {{{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
        std::shuffle(horiz.begin(), horiz.end(), rng);

        for (const auto& [dx, dy, dz] : horiz) {
            VoxelCell target = readCell(pos, lx + dx, ly + dy, lz + dz, grid, ghosts);
            if (target.materialId == material_ids::AIR) {
                writeSwap(pos, lx, ly, lz, lx + dx, ly + dy, lz + dz, cell, target, grid, tracker, boundaryWrites,
                          cellSwaps);
                return true;
            }
        }
        return false;
    });
}

bool FallingSandSystem::simulateChunk(ChunkCoord pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                                      ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng,
                                      BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps) {
    bool gravityChanged = simulateGravity(pos, grid, ghosts, tracker, frameIndex, rng, boundaryWrites, cellSwaps);
    bool liquidChanged = simulateLiquid(pos, grid, ghosts, tracker, frameIndex, rng, boundaryWrites, cellSwaps);
    bool settled = !gravityChanged && !liquidChanged;
    if (settled)
        tracker.putToSleep(pos);
    return settled;
}

} // namespace recurse::simulation

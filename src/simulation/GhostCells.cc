#include "fabric/simulation/GhostCells.hh"

namespace fabric::simulation {

using recurse::kChunkSize;

// -- GhostCellStore -----------------------------------------------------------

VoxelCell GhostCellStore::get(Face face, int u, int v) const {
    return faces[static_cast<int>(face)][u * kChunkSize + v];
}

void GhostCellStore::set(Face face, int u, int v, VoxelCell cell) {
    faces[static_cast<int>(face)][u * kChunkSize + v] = cell;
}

// -- GhostCellManager ---------------------------------------------------------

static int localIndex(int lx, int ly, int lz) {
    return lx + ly * kChunkSize + lz * kChunkSize * kChunkSize;
}

/// Read a single cell from a neighbor's read buffer, or return Air if missing.
static VoxelCell readNeighborCell(const SimulationGrid& grid, int cx, int cy, int cz, int lx, int ly, int lz) {
    const auto* buf = grid.readBuffer(cx, cy, cz);
    if (!buf)
        return VoxelCell{};
    return (*buf)[localIndex(lx, ly, lz)];
}

void GhostCellManager::syncGhostCells(ChunkPos pos, const SimulationGrid& grid) {
    auto& store = stores_[pos];

    // +X face: neighbor (cx+1), sample local x=0, u=ly, v=lz
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            store.set(Face::PosX, ly, lz, readNeighborCell(grid, pos.x + 1, pos.y, pos.z, 0, ly, lz));
        }
    }

    // -X face: neighbor (cx-1), sample local x=31, u=ly, v=lz
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            store.set(Face::NegX, ly, lz, readNeighborCell(grid, pos.x - 1, pos.y, pos.z, kChunkSize - 1, ly, lz));
        }
    }

    // +Y face: neighbor (cy+1), sample local y=0, u=lx, v=lz
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            store.set(Face::PosY, lx, lz, readNeighborCell(grid, pos.x, pos.y + 1, pos.z, lx, 0, lz));
        }
    }

    // -Y face: neighbor (cy-1), sample local y=31, u=lx, v=lz
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            store.set(Face::NegY, lx, lz, readNeighborCell(grid, pos.x, pos.y - 1, pos.z, lx, kChunkSize - 1, lz));
        }
    }

    // +Z face: neighbor (cz+1), sample local z=0, u=lx, v=ly
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int ly = 0; ly < kChunkSize; ++ly) {
            store.set(Face::PosZ, lx, ly, readNeighborCell(grid, pos.x, pos.y, pos.z + 1, lx, ly, 0));
        }
    }

    // -Z face: neighbor (cz-1), sample local z=31, u=lx, v=ly
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int ly = 0; ly < kChunkSize; ++ly) {
            store.set(Face::NegZ, lx, ly, readNeighborCell(grid, pos.x, pos.y, pos.z - 1, lx, ly, kChunkSize - 1));
        }
    }
}

void GhostCellManager::syncAll(const std::vector<ChunkPos>& chunks, const SimulationGrid& grid) {
    for (const auto& pos : chunks)
        syncGhostCells(pos, grid);
}

VoxelCell GhostCellManager::readGhost(ChunkPos pos, int lx, int ly, int lz) const {
    auto it = stores_.find(pos);
    if (it == stores_.end())
        return VoxelCell{};

    const auto& store = it->second;

    if (lx == -1)
        return store.get(Face::NegX, ly, lz);
    if (lx == kChunkSize)
        return store.get(Face::PosX, ly, lz);
    if (ly == -1)
        return store.get(Face::NegY, lx, lz);
    if (ly == kChunkSize)
        return store.get(Face::PosY, lx, lz);
    if (lz == -1)
        return store.get(Face::NegZ, lx, ly);
    if (lz == kChunkSize)
        return store.get(Face::PosZ, lx, ly);

    // Not out-of-bounds -- caller error, return Air
    return VoxelCell{};
}

GhostCellStore& GhostCellManager::getStore(ChunkPos pos) {
    return stores_[pos];
}

void GhostCellManager::remove(ChunkPos pos) {
    stores_.erase(pos);
}

} // namespace fabric::simulation

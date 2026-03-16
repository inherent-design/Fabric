#include "recurse/persistence/RollbackExecutor.hh"
#include "fabric/log/Log.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <cstring>
#include <unordered_set>

namespace recurse {

RollbackExecutor::RollbackExecutor(WorldTransactionStore& txStore, simulation::SimulationGrid& grid)
    : txStore_(txStore), grid_(grid) {}

RollbackResult RollbackExecutor::execute(const RollbackSpec& spec) {
    ChangeQuery query{};
    query.chunkRange = spec.chunkRange;
    query.fromTime = spec.targetTime;
    query.toTime = INT64_MAX;
    query.playerId = 0;
    query.limit = 100000;

    auto changes = txStore_.queryChanges(query);
    return applyReverse(changes);
}

RollbackResult RollbackExecutor::executePlayerOnly(const RollbackSpec& spec) {
    ChangeQuery query{};
    query.chunkRange = spec.chunkRange;
    query.fromTime = spec.targetTime;
    query.toTime = INT64_MAX;
    query.playerId = spec.playerId;
    query.limit = 100000;

    auto changes = txStore_.queryChanges(query);
    return applyReverse(changes);
}

RollbackResult RollbackExecutor::applyReverse(const std::vector<VoxelChange>& changes) {
    std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> seen;
    int64_t count = 0;

    for (const auto& c : changes) {
        int wx = c.addr.cx * recurse::simulation::K_CHUNK_SIZE + c.addr.vx;
        int wy = c.addr.cy * recurse::simulation::K_CHUNK_SIZE + c.addr.vy;
        int wz = c.addr.cz * recurse::simulation::K_CHUNK_SIZE + c.addr.vz;

        simulation::VoxelCell cell{};
        std::memcpy(&cell, &c.oldCell, sizeof(cell));

        grid_.writeCellImmediate(wx, wy, wz, cell);
        seen.insert(fabric::ChunkCoord{c.addr.cx, c.addr.cy, c.addr.cz});
        ++count;
    }

    RollbackResult result{};
    result.affectedChunks.reserve(seen.size());
    for (const auto& coord : seen) {
        result.affectedChunks.push_back(coord);
    }
    result.changesReverted = count;

    FABRIC_LOG_INFO("RollbackExecutor: reverted {} changes across {} chunks", count, seen.size());
    return result;
}

} // namespace recurse

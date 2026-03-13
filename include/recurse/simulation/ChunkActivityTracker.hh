#pragma once
#include "fabric/world/ChunkCoord.hh"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace recurse::simulation {

using fabric::ChunkCoord;
using fabric::ChunkCoordHash;

enum class ChunkState : uint8_t {
    Sleeping,
    Active,
    BoundaryDirty
};
enum class SimPriority : uint8_t {
    Immediate = 0,
    Normal = 1,
    Background = 2,
    Hibernating = 3
};

struct ActiveChunkEntry {
    ChunkCoord pos;
    SimPriority priority;
};

class ChunkActivityTracker {
  public:
    void setState(ChunkCoord pos, ChunkState state);
    ChunkState getState(ChunkCoord pos) const;
    void markSubRegionActive(ChunkCoord pos, int lx, int ly, int lz);
    uint64_t getSubRegionMask(ChunkCoord pos) const;
    void clearSubRegionMask(ChunkCoord pos);
    void notifyBoundaryChange(ChunkCoord neighborPos);
    void setReferencePoint(int wx, int wy, int wz);
    std::vector<ActiveChunkEntry> collectActiveChunks(int budgetCap = 0) const;
    void putToSleep(ChunkCoord pos);
    void resolveBoundaryDirty(ChunkCoord pos, bool needsSimulation);
    void remove(ChunkCoord pos);
    void clear(); // Remove all chunks (for world reset)

  private:
    struct ChunkInfo {
        ChunkState state = ChunkState::Sleeping;
        uint64_t subRegionMask = 0;
    };

    std::unordered_map<ChunkCoord, ChunkInfo, ChunkCoordHash> chunks_;
    int refX_ = 0, refY_ = 0, refZ_ = 0;

    SimPriority computePriority(ChunkCoord pos) const;
};

} // namespace recurse::simulation

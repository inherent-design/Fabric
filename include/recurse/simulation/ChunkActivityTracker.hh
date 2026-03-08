#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace recurse::simulation {

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

struct ChunkPos {
    int x, y, z;
    bool operator==(const ChunkPos&) const = default;
};

struct ChunkPosHash {
    size_t operator()(const ChunkPos& p) const;
};

struct ActiveChunkEntry {
    ChunkPos pos;
    SimPriority priority;
};

class ChunkActivityTracker {
  public:
    void setState(ChunkPos pos, ChunkState state);
    ChunkState getState(ChunkPos pos) const;
    void markSubRegionActive(ChunkPos pos, int lx, int ly, int lz);
    uint64_t getSubRegionMask(ChunkPos pos) const;
    void clearSubRegionMask(ChunkPos pos);
    void notifyBoundaryChange(ChunkPos neighborPos);
    void setReferencePoint(int wx, int wy, int wz);
    std::vector<ActiveChunkEntry> collectActiveChunks(int budgetCap = 0) const;
    void putToSleep(ChunkPos pos);
    void resolveBoundaryDirty(ChunkPos pos, bool needsSimulation);
    void remove(ChunkPos pos);
    void clear(); // Remove all chunks (for world reset)

  private:
    struct ChunkInfo {
        ChunkState state = ChunkState::Sleeping;
        uint64_t subRegionMask = 0;
    };

    std::unordered_map<ChunkPos, ChunkInfo, ChunkPosHash> chunks_;
    int refX_ = 0, refY_ = 0, refZ_ = 0;

    SimPriority computePriority(ChunkPos pos) const;
};

} // namespace recurse::simulation

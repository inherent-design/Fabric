#include "recurse/simulation/ChunkRegistry.hh"
#include <cassert>

namespace recurse::simulation {

namespace {
bool isValidTransition(ChunkSlotState from, ChunkSlotState to) {
    return (from == ChunkSlotState::Absent && to == ChunkSlotState::Generating) ||
           (from == ChunkSlotState::Generating && to == ChunkSlotState::Active) ||
           (from == ChunkSlotState::Active && to == ChunkSlotState::Draining) ||
           (from == ChunkSlotState::Draining && to == ChunkSlotState::Absent);
}
} // namespace

// -- ChunkBuffers -------------------------------------------------------------

void ChunkBuffers::materialize() {
    if (isMaterialized())
        return;
    for (int i = 0; i < K_COUNT; ++i) {
        buffers[i] = std::make_unique<Buffer>();
        buffers[i]->fill(fillValue);
    }
}

bool ChunkBuffers::isMaterialized() const {
    return buffers[0] != nullptr;
}

// -- ChunkRegistry ------------------------------------------------------------

ChunkSlot& ChunkRegistry::addChunk(int cx, int cy, int cz) {
    auto key = packChunkKey(cx, cy, cz);
    auto it = slots_.find(key);
    if (it != slots_.end())
        return it->second;
    return slots_[key];
}

void ChunkRegistry::removeChunk(int cx, int cy, int cz) {
    slots_.erase(packChunkKey(cx, cy, cz));
}

void ChunkRegistry::transitionState(int cx, int cy, int cz, ChunkSlotState to) {
    auto* slot = find(cx, cy, cz);
    assert(slot && "transitionState: slot must exist");
    assert(isValidTransition(slot->state, to) && "transitionState: invalid state transition");
    slot->state = to;
}

void ChunkRegistry::clear() {
    slots_.clear();
}

ChunkSlot* ChunkRegistry::find(int cx, int cy, int cz) {
    auto it = slots_.find(packChunkKey(cx, cy, cz));
    if (it == slots_.end())
        return nullptr;
    return &it->second;
}

const ChunkSlot* ChunkRegistry::find(int cx, int cy, int cz) const {
    auto it = slots_.find(packChunkKey(cx, cy, cz));
    if (it == slots_.end())
        return nullptr;
    return &it->second;
}

bool ChunkRegistry::hasChunk(int cx, int cy, int cz) const {
    return slots_.find(packChunkKey(cx, cy, cz)) != slots_.end();
}

size_t ChunkRegistry::chunkCount() const {
    return slots_.size();
}

size_t ChunkRegistry::materializedChunkCount() const {
    size_t count = 0;
    for (const auto& [_, slot] : slots_) {
        if (slot.isMaterialized())
            ++count;
    }
    return count;
}

std::vector<std::tuple<int, int, int>> ChunkRegistry::allChunks() const {
    std::vector<std::tuple<int, int, int>> result;
    result.reserve(slots_.size());
    for (const auto& [key, _] : slots_) {
        auto [cx, cy, cz] = unpackChunkKey(key);
        result.emplace_back(cx, cy, cz);
    }
    return result;
}

// -- Dispatch helpers (C-1c) --------------------------------------------------

std::vector<ChunkDispatchEntry> ChunkRegistry::buildDispatchList(ChunkSlotState filter) {
    std::vector<ChunkDispatchEntry> result;
    for (auto& [key, slot] : slots_) {
        if (slot.state == filter) {
            auto [cx, cy, cz] = unpackChunkKey(key);
            result.push_back({ChunkPos{cx, cy, cz}, &slot});
        }
    }
    return result;
}

void ChunkRegistry::resolveBufferPointers(uint64_t epoch) {
    constexpr int K = ChunkBuffers::K_COUNT;
    for (auto& [key, slot] : slots_) {
        if (slot.isMaterialized()) {
            slot.readPtr = slot.simBuffers.buffers[epoch % K].get()->data();
            slot.writePtr = slot.simBuffers.buffers[(epoch + 1) % K].get()->data();
        } else {
            slot.readPtr = nullptr;
            slot.writePtr = nullptr;
        }
    }
}

} // namespace recurse::simulation

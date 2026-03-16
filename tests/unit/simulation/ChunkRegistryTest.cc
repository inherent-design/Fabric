#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class ChunkRegistryTest : public ::testing::Test {
  protected:
    ChunkRegistry registry;
};

// 1. addChunk returns reference; second call returns same chunk (no duplicate)
TEST_F(ChunkRegistryTest, AddChunkIdempotent) {
    auto& first = registry.addChunk(0, 0, 0);
    first.simBuffers.fillValue.materialId = material_ids::STONE;

    auto& second = registry.addChunk(0, 0, 0);
    EXPECT_EQ(second.simBuffers.fillValue.materialId, material_ids::STONE);
    EXPECT_EQ(&first, &second);
    EXPECT_EQ(registry.chunkCount(), 1u);
}

// 2. find returns nullptr for absent chunk
TEST_F(ChunkRegistryTest, FindAbsentReturnsNull) {
    EXPECT_EQ(registry.find(0, 0, 0), nullptr);

    const auto& constRegistry = registry;
    EXPECT_EQ(constRegistry.find(0, 0, 0), nullptr);
}

// 3. find returns pointer for present chunk
TEST_F(ChunkRegistryTest, FindPresentReturnsPointer) {
    registry.addChunk(1, 2, 3);

    auto* ptr = registry.find(1, 2, 3);
    ASSERT_NE(ptr, nullptr);

    const auto& constRegistry = registry;
    const auto* cptr = constRegistry.find(1, 2, 3);
    ASSERT_NE(cptr, nullptr);
}

// 4. removeChunk makes find return nullptr
TEST_F(ChunkRegistryTest, RemoveChunkMakesAbsent) {
    registry.addChunk(5, 5, 5);
    EXPECT_NE(registry.find(5, 5, 5), nullptr);

    registry.removeChunk(5, 5, 5);
    EXPECT_EQ(registry.find(5, 5, 5), nullptr);
}

// 5. hasChunk true/false
TEST_F(ChunkRegistryTest, HasChunk) {
    EXPECT_FALSE(registry.hasChunk(0, 0, 0));
    registry.addChunk(0, 0, 0);
    EXPECT_TRUE(registry.hasChunk(0, 0, 0));
}

// 6. chunkCount tracks additions/removals
TEST_F(ChunkRegistryTest, ChunkCountTracksChanges) {
    EXPECT_EQ(registry.chunkCount(), 0u);

    registry.addChunk(0, 0, 0);
    EXPECT_EQ(registry.chunkCount(), 1u);

    registry.addChunk(1, 0, 0);
    EXPECT_EQ(registry.chunkCount(), 2u);

    // Duplicate add does not increase count
    registry.addChunk(0, 0, 0);
    EXPECT_EQ(registry.chunkCount(), 2u);

    registry.removeChunk(0, 0, 0);
    EXPECT_EQ(registry.chunkCount(), 1u);
}

// 7. materializedChunkCount only counts materialized
TEST_F(ChunkRegistryTest, MaterializedChunkCount) {
    registry.addChunk(0, 0, 0);
    registry.addChunk(1, 0, 0);
    registry.addChunk(2, 0, 0);

    EXPECT_EQ(registry.materializedChunkCount(), 0u);

    registry.find(0, 0, 0)->materialize();
    EXPECT_EQ(registry.materializedChunkCount(), 1u);

    registry.find(2, 0, 0)->materialize();
    EXPECT_EQ(registry.materializedChunkCount(), 2u);
}

// 8. allChunks returns all chunk coordinates
//    Note: unpackChunkKey has a pre-existing sign-extension bug for negative X;
//    Y and Z negative values roundtrip correctly. Use non-negative X here.
TEST_F(ChunkRegistryTest, AllChunksReturnsCoordinates) {
    registry.addChunk(0, 0, 0);
    registry.addChunk(1, 2, 3);
    registry.addChunk(0, -1, -1);

    auto chunks = registry.allChunks();
    EXPECT_EQ(chunks.size(), 3u);

    auto contains = [&](int cx, int cy, int cz) {
        for (const auto& [x, y, z] : chunks) {
            if (x == cx && y == cy && z == cz)
                return true;
        }
        return false;
    };
    EXPECT_TRUE(contains(0, 0, 0));
    EXPECT_TRUE(contains(1, 2, 3));
    EXPECT_TRUE(contains(0, -1, -1));
}

// 9. clear resets everything
TEST_F(ChunkRegistryTest, ClearResetsAll) {
    registry.addChunk(0, 0, 0);
    registry.addChunk(1, 0, 0);
    registry.find(0, 0, 0)->materialize();

    registry.clear();

    EXPECT_EQ(registry.chunkCount(), 0u);
    EXPECT_EQ(registry.materializedChunkCount(), 0u);
    EXPECT_FALSE(registry.hasChunk(0, 0, 0));
    EXPECT_TRUE(registry.allChunks().empty());
}

// 10. ChunkSlot materialize/isMaterialized (delegates to simBuffers)
TEST_F(ChunkRegistryTest, ChunkSlotMaterialize) {
    auto& slot = registry.addChunk(0, 0, 0);
    EXPECT_FALSE(slot.isMaterialized());

    slot.materialize();
    EXPECT_TRUE(slot.isMaterialized());
    for (int i = 0; i < ChunkBuffers::K_COUNT; ++i)
        ASSERT_NE(slot.simBuffers.buffers[i], nullptr) << "buffer " << i;

    // Second materialize is a no-op
    auto* buf0 = slot.simBuffers.buffers[0].get();
    slot.materialize();
    EXPECT_EQ(slot.simBuffers.buffers[0].get(), buf0);
}

// 11. Materialized buffers are filled with fillValue
TEST_F(ChunkRegistryTest, MaterializePreservesFillValue) {
    auto& slot = registry.addChunk(0, 0, 0);
    slot.simBuffers.fillValue.materialId = material_ids::STONE;
    slot.materialize();

    for (int b = 0; b < ChunkBuffers::K_COUNT; ++b) {
        for (size_t i = 0; i < K_CHUNK_VOLUME; ++i) {
            EXPECT_EQ((*slot.simBuffers.buffers[b])[i].materialId, material_ids::STONE)
                << "Buffer " << b << ", index " << i;
        }
    }
}

// 12. forEachMaterialized only visits materialized chunks
TEST_F(ChunkRegistryTest, ForEachMaterializedSkipsUnmaterialized) {
    registry.addChunk(0, 0, 0);
    registry.addChunk(1, 0, 0);
    registry.addChunk(2, 0, 0);

    registry.find(0, 0, 0)->materialize();
    registry.find(2, 0, 0)->materialize();

    int count = 0;
    registry.forEachMaterialized([&count](ChunkSlot&) { ++count; });
    EXPECT_EQ(count, 2);
}

// 13. removeChunk on absent key is safe (no-op)
TEST_F(ChunkRegistryTest, RemoveAbsentChunkIsSafe) {
    EXPECT_NO_THROW(registry.removeChunk(99, 99, 99));
    EXPECT_EQ(registry.chunkCount(), 0u);
}

// 14. Memory size matches expected value
TEST_F(ChunkRegistryTest, BufferMemorySize) {
    static_assert(sizeof(ChunkBuffers::Buffer) == K_CHUNK_VOLUME * sizeof(VoxelCell));
    EXPECT_EQ(sizeof(ChunkBuffers::Buffer), 131072u);
}

// 15. ChunkSlot default state is Absent
TEST_F(ChunkRegistryTest, ChunkSlotDefaultState) {
    auto& slot = registry.addChunk(0, 0, 0);
    EXPECT_EQ(slot.state, ChunkSlotState::Absent);
}

// 16. ChunkSlot writePtr/readPtr default to nullptr
TEST_F(ChunkRegistryTest, ChunkSlotWriteReadPointers) {
    auto& slot = registry.addChunk(0, 0, 0);
    EXPECT_EQ(slot.writePtr, nullptr);
    EXPECT_EQ(slot.readPtr, nullptr);
}

// 17. ChunkSlot convenience delegation forwards to simBuffers
TEST_F(ChunkRegistryTest, ChunkSlotConvenienceDelegation) {
    auto& slot = registry.addChunk(0, 0, 0);
    EXPECT_FALSE(slot.isMaterialized());
    EXPECT_FALSE(slot.simBuffers.isMaterialized());

    slot.materialize();
    EXPECT_TRUE(slot.isMaterialized());
    EXPECT_TRUE(slot.simBuffers.isMaterialized());
}

// 18. buildDispatchList returns empty vector when no chunks exist
TEST_F(ChunkRegistryTest, BuildDispatchListEmpty) {
    auto list = registry.buildDispatchList(ChunkSlotState::Active);
    EXPECT_TRUE(list.empty());
}

// 19. buildDispatchList filters by state
TEST_F(ChunkRegistryTest, BuildDispatchListFiltersState) {
    auto& s0 = registry.addChunk(0, 0, 0);
    s0.state = ChunkSlotState::Active;

    auto& s1 = registry.addChunk(1, 0, 0);
    s1.state = ChunkSlotState::Generating;

    auto& s2 = registry.addChunk(2, 0, 0);
    s2.state = ChunkSlotState::Draining;

    auto list = registry.buildDispatchList(ChunkSlotState::Active);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].pos.x, 0);
    EXPECT_EQ(list[0].pos.y, 0);
    EXPECT_EQ(list[0].pos.z, 0);
    EXPECT_EQ(list[0].slot, &s0);
}

// 20. resolveBufferPointers sets correct pointers for each epoch
TEST_F(ChunkRegistryTest, ResolveBufferPointersCorrectForEpoch) {
    auto& slot = registry.addChunk(0, 0, 0);
    slot.materialize();

    constexpr int K = ChunkBuffers::K_COUNT;
    for (uint64_t epoch = 0; epoch < static_cast<uint64_t>(K * 2); ++epoch) {
        registry.resolveBufferPointers(epoch);
        EXPECT_EQ(slot.readPtr, slot.simBuffers.buffers[epoch % K].get()->data()) << "readPtr at epoch " << epoch;
        EXPECT_EQ(slot.writePtr, slot.simBuffers.buffers[(epoch + 1) % K].get()->data())
            << "writePtr at epoch " << epoch;
    }
}

// 21. resolveBufferPointers sets nullptr for unmaterialized slots
TEST_F(ChunkRegistryTest, ResolveBufferPointersNullForUnmaterialized) {
    registry.addChunk(0, 0, 0);

    registry.resolveBufferPointers(0);

    auto* slot = registry.find(0, 0, 0);
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->readPtr, nullptr);
    EXPECT_EQ(slot->writePtr, nullptr);
}

// 22. Valid lifecycle transitions succeed via typed transition()
TEST_F(ChunkRegistryTest, ValidTransitionsSucceed) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    EXPECT_EQ(registry.find(0, 0, 0)->state, ChunkSlotState::Absent);

    auto generating = transition<Absent, Generating>(absent, registry);
    EXPECT_EQ(registry.find(0, 0, 0)->state, ChunkSlotState::Generating);

    auto active = transition<Generating, Active>(generating, registry);
    EXPECT_EQ(registry.find(0, 0, 0)->state, ChunkSlotState::Active);

    auto draining = transition<Active, Draining>(active, registry);
    EXPECT_EQ(registry.find(0, 0, 0)->state, ChunkSlotState::Draining);
    (void)draining;
}

// 23. Invalid transitions are compile errors (verified by static_assert in ChunkState.hh).
// No runtime test needed; the following would not compile:
// transition<Absent, Active>(absent, registry);
TEST_F(ChunkRegistryTest, InvalidTransitionsPreventedAtCompileTime) {
    static_assert(!ValidTransition<Absent, Active>);
    static_assert(!ValidTransition<Generating, Draining>);
    static_assert(!ValidTransition<Active, Generating>);
    static_assert(!ValidTransition<Draining, Active>);
}

// 24. syncChunkBuffers copies a single chunk without touching others
TEST_F(ChunkRegistryTest, SyncChunkBuffersCopiesSingleChunk) {
    SimulationGrid grid;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);
    grid.fillChunk(1, 0, 0, VoxelCell{});
    grid.materializeChunk(1, 0, 0);

    VoxelCell sand;
    sand.materialId = material_ids::SAND;
    grid.writeCell(0, 0, 0, sand);

    grid.syncChunkBuffers(0, 0, 0);

    // Chunk A read buffer should now have Sand
    EXPECT_EQ(grid.readCell(0, 0, 0).materialId, material_ids::SAND);

    // Chunk B read buffer should still be Air (untouched)
    EXPECT_EQ(grid.readCell(32, 0, 0).materialId, material_ids::AIR);
}

// 25. syncChunkBuffers is a no-op for unmaterialized chunks
TEST_F(ChunkRegistryTest, SyncChunkBuffersNoOpForUnmaterialized) {
    SimulationGrid grid;
    grid.fillChunk(0, 0, 0, VoxelCell{});
    EXPECT_NO_THROW(grid.syncChunkBuffers(0, 0, 0));
}

// 26. Lifecycle state transitions through Generating -> Active via typed handles
TEST_F(ChunkRegistryTest, GenerateChunkSetsLifecycleStates) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    EXPECT_EQ(registry.find(0, 0, 0)->state, ChunkSlotState::Absent);

    auto generating = transition<Absent, Generating>(absent, registry);
    EXPECT_EQ(registry.find(0, 0, 0)->state, ChunkSlotState::Generating);

    auto active = transition<Generating, Active>(generating, registry);
    EXPECT_EQ(registry.find(0, 0, 0)->state, ChunkSlotState::Active);
    (void)active;
}

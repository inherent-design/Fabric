#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class ChunkStateTest : public ::testing::Test {
  protected:
    ChunkRegistry registry;
};

TEST_F(ChunkStateTest, AddChunkRefReturnsAbsent) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    EXPECT_EQ(absent.runtimeState(), ChunkSlotState::Absent);
    EXPECT_EQ(absent.cx(), 0);
    EXPECT_EQ(absent.cy(), 0);
    EXPECT_EQ(absent.cz(), 0);
}

TEST_F(ChunkStateTest, TransitionAbsentToGenerating) {
    auto absent = addChunkRef(registry, 1, 2, 3);
    auto generating = transition<Absent, Generating>(absent, registry);
    EXPECT_EQ(generating.runtimeState(), ChunkSlotState::Generating);
    EXPECT_EQ(generating.cx(), 1);
    EXPECT_EQ(generating.cy(), 2);
    EXPECT_EQ(generating.cz(), 3);
}

TEST_F(ChunkStateTest, TransitionGeneratingToActive) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    auto generating = transition<Absent, Generating>(absent, registry);
    auto active = transition<Generating, Active>(generating, registry);
    EXPECT_EQ(active.runtimeState(), ChunkSlotState::Active);
}

TEST_F(ChunkStateTest, TransitionActiveToDraining) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    auto generating = transition<Absent, Generating>(absent, registry);
    auto active = transition<Generating, Active>(generating, registry);
    auto draining = transition<Active, Draining>(active, registry);
    EXPECT_EQ(draining.runtimeState(), ChunkSlotState::Draining);
}

TEST_F(ChunkStateTest, FindAsReturnsCorrectType) {
    auto absent = addChunkRef(registry, 5, 0, 5);
    auto generating = transition<Absent, Generating>(absent, registry);
    transition<Generating, Active>(generating, registry);

    auto found = findAs<Active>(registry, 5, 0, 5);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->cx(), 5);
    EXPECT_EQ(found->cz(), 5);
}

TEST_F(ChunkStateTest, FindAsWrongStateReturnsNullopt) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    transition<Absent, Generating>(absent, registry);

    EXPECT_FALSE(findAs<Active>(registry, 0, 0, 0).has_value());
    EXPECT_TRUE(findAs<Generating>(registry, 0, 0, 0).has_value());
}

TEST_F(ChunkStateTest, FindAsNonexistentReturnsNullopt) {
    EXPECT_FALSE(findAs<Active>(registry, 99, 99, 99).has_value());
}

TEST_F(ChunkStateTest, CancelAndRemoveErasesSlot) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    auto generating = transition<Absent, Generating>(absent, registry);
    cancelAndRemove(generating, registry);
    EXPECT_FALSE(registry.hasChunk(0, 0, 0));
}

TEST_F(ChunkStateTest, FullLifecycleUpdatesRuntimeState) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    EXPECT_EQ(absent.runtimeState(), ChunkSlotState::Absent);

    auto generating = transition<Absent, Generating>(absent, registry);
    EXPECT_EQ(generating.runtimeState(), ChunkSlotState::Generating);

    auto active = transition<Generating, Active>(generating, registry);
    EXPECT_EQ(active.runtimeState(), ChunkSlotState::Active);

    auto draining = transition<Active, Draining>(active, registry);
    EXPECT_EQ(draining.runtimeState(), ChunkSlotState::Draining);
}

TEST_F(ChunkStateTest, ChunkRefSizeIsMinimal) {
    // 12 bytes (ChunkCoord) + 4 padding + 8 bytes (pointer) = 24
    static_assert(sizeof(ChunkRef<Active>) == 24);
    static_assert(sizeof(ChunkRef<Absent>) == 24);
    static_assert(sizeof(ChunkRef<Generating>) == 24);
    static_assert(sizeof(ChunkRef<Draining>) == 24);
}

TEST_F(ChunkStateTest, ValidTransitionCompileTimeChecks) {
    static_assert(ValidTransition<Absent, Generating>);
    static_assert(ValidTransition<Generating, Active>);
    static_assert(ValidTransition<Active, Draining>);

    static_assert(!ValidTransition<Absent, Active>);
    static_assert(!ValidTransition<Absent, Draining>);
    static_assert(!ValidTransition<Generating, Draining>);
    static_assert(!ValidTransition<Active, Generating>);
    static_assert(!ValidTransition<Draining, Active>);
    static_assert(!ValidTransition<Draining, Generating>);
    static_assert(!ValidTransition<Draining, Absent>);
}

TEST_F(ChunkStateTest, StateForMapping) {
    EXPECT_EQ(stateFor<Absent>(), ChunkSlotState::Absent);
    EXPECT_EQ(stateFor<Generating>(), ChunkSlotState::Generating);
    EXPECT_EQ(stateFor<Active>(), ChunkSlotState::Active);
    EXPECT_EQ(stateFor<Draining>(), ChunkSlotState::Draining);
}

TEST_F(ChunkStateTest, GeneratingRefCanMaterialize) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    auto generating = transition<Absent, Generating>(absent, registry);
    EXPECT_FALSE(generating.isMaterialized());
    generating.materialize();
    EXPECT_TRUE(generating.isMaterialized());
}

TEST_F(ChunkStateTest, ActiveRefCanMarkDirty) {
    auto absent = addChunkRef(registry, 0, 0, 0);
    auto generating = transition<Absent, Generating>(absent, registry);
    generating.materialize();
    auto active = transition<Generating, Active>(generating, registry);
    active.markDirty();
    // copyCountdown should be K_COUNT - 1 = 2
    auto* slot = registry.find(0, 0, 0);
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->copyCountdown, ChunkBuffers::K_COUNT - 1);
}

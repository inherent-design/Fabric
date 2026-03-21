#include "recurse/simulation/MatterState.hh"
#include "recurse/simulation/CellAccessors.hh"

#include <gtest/gtest.h>

using namespace recurse::simulation;

// -- MatterState layout -----------------------------------------------------

TEST(MatterStateTest, SizeIs4Bytes) {
    EXPECT_EQ(sizeof(MatterState), 4u);
}

TEST(MatterStateTest, DefaultIsAllZero) {
    MatterState cell;
    EXPECT_EQ(cell.essenceIdx, 0);
    EXPECT_EQ(cell.displacementRank, 0);
    EXPECT_EQ(cell.phaseAndFlags, 0);
    EXPECT_EQ(cell.spare, 0);
}

TEST(MatterStateTest, DefaultPhaseIsEmpty) {
    MatterState cell;
    EXPECT_EQ(cell.phase(), Phase::Empty);
}

// -- Phase round-trip -------------------------------------------------------

TEST(MatterStateTest, SetPhaseRoundTrip) {
    MatterState cell;
    const Phase phases[] = {Phase::Empty, Phase::Solid, Phase::Powder, Phase::Liquid, Phase::Gas};
    for (auto p : phases) {
        cell.setPhase(p);
        EXPECT_EQ(cell.phase(), p);
    }
}

TEST(MatterStateTest, SetPhaseDoesNotCorruptFlags) {
    MatterState cell;
    cell.setFlags(0x1F); // all 5 flag bits set
    cell.setPhase(Phase::Liquid);
    EXPECT_EQ(cell.phase(), Phase::Liquid);
    EXPECT_EQ(cell.flags(), 0x1F);
}

// -- Flags round-trip -------------------------------------------------------

TEST(MatterStateTest, SetFlagsRoundTrip) {
    MatterState cell;
    for (uint8_t f = 0; f <= 0x1F; ++f) {
        cell.setFlags(f);
        EXPECT_EQ(cell.flags(), f);
    }
}

TEST(MatterStateTest, SetFlagsDoesNotCorruptPhase) {
    MatterState cell;
    cell.setPhase(Phase::Gas);
    cell.setFlags(0x15);
    EXPECT_EQ(cell.phase(), Phase::Gas);
    EXPECT_EQ(cell.flags(), 0x15);
}

// -- Field isolation --------------------------------------------------------

TEST(MatterStateTest, DisplacementRankIsolated) {
    MatterState cell;
    cell.displacementRank = 200;
    cell.setPhase(Phase::Powder);
    cell.setFlags(0x0A);
    EXPECT_EQ(cell.displacementRank, 200);
    EXPECT_EQ(cell.essenceIdx, 0);
    EXPECT_EQ(cell.spare, 0);
}

TEST(MatterStateTest, EssenceIdxIsolated) {
    MatterState cell;
    cell.essenceIdx = 42;
    cell.displacementRank = 130;
    cell.setPhase(Phase::Solid);
    EXPECT_EQ(cell.essenceIdx, 42);
    EXPECT_EQ(cell.displacementRank, 130);
}

// -- CellAccessors: isOccupied / isEmpty for MatterState --------------------

TEST(MatterStateTest, EmptyPhaseIsEmpty) {
    MatterState cell;
    cell.setPhase(Phase::Empty);
    EXPECT_TRUE(isEmpty(cell));
    EXPECT_FALSE(isOccupied(cell));
}

TEST(MatterStateTest, NonEmptyPhaseIsOccupied) {
    MatterState cell;
    const Phase occupied[] = {Phase::Solid, Phase::Powder, Phase::Liquid, Phase::Gas};
    for (auto p : occupied) {
        cell.setPhase(p);
        EXPECT_TRUE(isOccupied(cell));
        EXPECT_FALSE(isEmpty(cell));
    }
}

// -- CellAccessors: cellPhase for MatterState -------------------------------

TEST(MatterStateTest, CellPhaseMapping) {
    MaterialRegistry registry;
    MatterState cell;

    cell.setPhase(Phase::Solid);
    EXPECT_EQ(cellPhase(registry, cell), MoveType::Static);

    cell.setPhase(Phase::Powder);
    EXPECT_EQ(cellPhase(registry, cell), MoveType::Powder);

    cell.setPhase(Phase::Liquid);
    EXPECT_EQ(cellPhase(registry, cell), MoveType::Liquid);

    cell.setPhase(Phase::Gas);
    EXPECT_EQ(cellPhase(registry, cell), MoveType::Gas);

    cell.setPhase(Phase::Empty);
    EXPECT_EQ(cellPhase(registry, cell), MoveType::Static);
}

// -- CellAccessors: canDisplace for MatterState -----------------------------

TEST(MatterStateTest, CanDisplaceEmptyTarget) {
    MaterialRegistry registry;
    MatterState mover;
    mover.setPhase(Phase::Powder);
    mover.displacementRank = 100;

    MatterState target;
    target.setPhase(Phase::Empty);

    EXPECT_TRUE(canDisplace(registry, mover, target));
}

TEST(MatterStateTest, CannotDisplaceSolid) {
    MaterialRegistry registry;
    MatterState mover;
    mover.setPhase(Phase::Liquid);
    mover.displacementRank = 255;

    MatterState target;
    target.setPhase(Phase::Solid);
    target.displacementRank = 1;

    EXPECT_FALSE(canDisplace(registry, mover, target));
}

TEST(MatterStateTest, DisplacementRankOrdering) {
    MaterialRegistry registry;
    MatterState heavy;
    heavy.setPhase(Phase::Powder);
    heavy.displacementRank = 200;

    MatterState light;
    light.setPhase(Phase::Liquid);
    light.displacementRank = 100;

    EXPECT_TRUE(canDisplace(registry, heavy, light));
    EXPECT_FALSE(canDisplace(registry, light, heavy));
}

TEST(MatterStateTest, EqualRankCannotDisplace) {
    MaterialRegistry registry;
    MatterState a;
    a.setPhase(Phase::Powder);
    a.displacementRank = 100;

    MatterState b;
    b.setPhase(Phase::Liquid);
    b.displacementRank = 100;

    EXPECT_FALSE(canDisplace(registry, a, b));
    EXPECT_FALSE(canDisplace(registry, b, a));
}

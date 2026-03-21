#include "recurse/world/TestWorldGenerator.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

// TerrainSystem tests need AppContext infrastructure
#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/resource/AssetRegistry.hh"
#include "fabric/resource/ResourceHub.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/world/MinecraftNoiseGenerator.hh"
#include "recurse/world/NaturalWorldGenerator.hh"

using namespace recurse::simulation;
using namespace recurse;
using recurse::simulation::K_CHUNK_SIZE;
using recurse::simulation::K_CHUNK_VOLUME;

// -- FlatWorldGenerator -------------------------------------------------------

TEST(TestWorldGenerator, FlatWorldGenerator_BelowGround_Stone) {
    SimulationGrid grid;
    FlatWorldGenerator gen(32);
    gen.generate(grid, 0, 0, 0);
    grid.advanceEpoch();

    // Chunk (0,0,0) spans y=[0,31], entirely below groundLevel=32
    for (int y = 0; y < K_CHUNK_SIZE; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, material_ids::STONE) << "y=" << y << " should be stone";
    }
}

TEST(TestWorldGenerator, FlatWorldGenerator_AboveGround_Air) {
    SimulationGrid grid;
    FlatWorldGenerator gen(32);
    gen.generate(grid, 0, 2, 0); // y range [64, 95]
    grid.advanceEpoch();

    for (int y = 64; y < 96; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, material_ids::AIR) << "y=" << y << " should be air";
    }
}

// -- LayeredWorldGenerator ----------------------------------------------------

TEST(TestWorldGenerator, LayeredWorldGenerator_StoneAndSand) {
    SimulationGrid grid;
    LayeredWorldGenerator gen(28, 4); // stone < 28, sand [28,31], air >= 32
    gen.generate(grid, 0, 0, 0);
    grid.advanceEpoch();

    // Stone region: y=[0,27]
    for (int y = 0; y < 28; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, material_ids::STONE) << "y=" << y << " should be stone";
    }
    // Sand region: y=[28,31]
    for (int y = 28; y < 32; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, material_ids::SAND) << "y=" << y << " should be sand";
    }
}

// -- FlatWorldGenerator sampleMaterial ----------------------------------------

TEST(TestWorldGenerator, FlatSampleMaterial_BelowGround_Stone) {
    FlatWorldGenerator gen(32);
    EXPECT_EQ(gen.sampleMaterial(0, 0, 0), material_ids::STONE);
    EXPECT_EQ(gen.sampleMaterial(5, 31, 10), material_ids::STONE);
}

TEST(TestWorldGenerator, FlatSampleMaterial_AtAndAboveGround_Air) {
    FlatWorldGenerator gen(32);
    EXPECT_EQ(gen.sampleMaterial(0, 32, 0), material_ids::AIR);
    EXPECT_EQ(gen.sampleMaterial(0, 100, 0), material_ids::AIR);
}

// -- LayeredWorldGenerator sampleMaterial -------------------------------------

TEST(TestWorldGenerator, LayeredSampleMaterial_StoneAndSandAndAir) {
    LayeredWorldGenerator gen(28, 4);
    EXPECT_EQ(gen.sampleMaterial(0, 0, 0), material_ids::STONE);
    EXPECT_EQ(gen.sampleMaterial(0, 27, 0), material_ids::STONE);
    EXPECT_EQ(gen.sampleMaterial(0, 28, 0), material_ids::SAND);
    EXPECT_EQ(gen.sampleMaterial(0, 31, 0), material_ids::SAND);
    EXPECT_EQ(gen.sampleMaterial(0, 32, 0), material_ids::AIR);
}

// -- maxSurfaceHeight ---------------------------------------------------------

TEST(TestWorldGenerator, FlatMaxSurfaceHeight_ReturnsGroundLevel) {
    FlatWorldGenerator gen(32);
    EXPECT_EQ(gen.maxSurfaceHeight(0, 0), 32);
    EXPECT_EQ(gen.maxSurfaceHeight(100, -50), 32);
}

TEST(TestWorldGenerator, LayeredMaxSurfaceHeight_ReturnsStonePlusSand) {
    LayeredWorldGenerator gen(28, 4);
    EXPECT_EQ(gen.maxSurfaceHeight(0, 0), 32);
    EXPECT_EQ(gen.maxSurfaceHeight(-10, 10), 32);
}

// -- Interface ----------------------------------------------------------------

TEST(TestWorldGenerator, WorldGeneratorInterface_Swappable) {
    SimulationGrid grid;

    // Generate with FlatWorldGenerator(32): below y=32 is stone
    FlatWorldGenerator flat(32);
    flat.generate(grid, 0, 0, 0);
    grid.advanceEpoch();
    EXPECT_EQ(grid.readCell(0, 28, 0).materialId, material_ids::STONE);

    // Re-generate same chunk with LayeredWorldGenerator(28, 4):
    // y=28 should now be sand instead of stone
    LayeredWorldGenerator layered(28, 4);
    layered.generate(grid, 0, 0, 0);
    grid.advanceEpoch();
    EXPECT_EQ(grid.readCell(0, 28, 0).materialId, material_ids::SAND);
}

TEST(TestWorldGenerator, GeneratorName_ReturnsCorrect) {
    FlatWorldGenerator flat;
    EXPECT_EQ(flat.name(), "FlatWorldGenerator");

    LayeredWorldGenerator layered;
    EXPECT_EQ(layered.name(), "LayeredWorldGenerator");
}

// -- TerrainSystem ------------------------------------------------------------

TEST(TestWorldGenerator, TerrainSystem_InitCreatesGrid) {
    fabric::World world;
    fabric::Timeline timeline;
    fabric::EventDispatcher dispatcher;
    fabric::ResourceHub hub;
    hub.disableWorkerThreadsForTesting();
    fabric::AssetRegistry assetRegistry{hub};
    fabric::SystemRegistry sysReg;
    fabric::ConfigManager configManager;

    sysReg.registerSystem<recurse::systems::TerrainSystem>(fabric::SystemPhase::FixedUpdate);
    ASSERT_TRUE(sysReg.resolve());

    fabric::AppContext ctx{
        .world = world,
        .timeline = timeline,
        .dispatcher = dispatcher,
        .resourceHub = hub,
        .assetRegistry = assetRegistry,
        .systemRegistry = sysReg,
        .configManager = configManager,
    };
    sysReg.initAll(ctx);

    auto* terrain = sysReg.get<recurse::systems::TerrainSystem>();
    ASSERT_NE(terrain, nullptr);
    // After init, worldGenerator should be valid
    EXPECT_NO_THROW(terrain->worldGenerator());
    EXPECT_EQ(terrain->worldGenerator().name(), "FlatWorldGenerator");

    sysReg.shutdownAll();
}

// -- generateToBuffer (C-P1) --------------------------------------------------

TEST(TestWorldGenerator, GenerateToBuffer_FlatBelowGround_AllStone) {
    FlatWorldGenerator gen(32);
    std::array<VoxelCell, K_CHUNK_VOLUME> buffer{};
    gen.generateToBuffer(buffer.data(), 0, 0, 0);

    for (int y = 0; y < K_CHUNK_SIZE; ++y) {
        int idx = 0 + y * K_CHUNK_SIZE + 0 * K_CHUNK_SIZE * K_CHUNK_SIZE;
        EXPECT_EQ(buffer[idx].materialId, material_ids::STONE) << "y=" << y;
    }
}

TEST(TestWorldGenerator, GenerateToBuffer_FlatAboveGround_AllAir) {
    FlatWorldGenerator gen(32);
    std::array<VoxelCell, K_CHUNK_VOLUME> buffer{};
    gen.generateToBuffer(buffer.data(), 0, 2, 0);

    for (size_t i = 0; i < K_CHUNK_VOLUME; ++i) {
        EXPECT_EQ(buffer[i].materialId, material_ids::AIR);
    }
}

TEST(TestWorldGenerator, GenerateToBuffer_FlatMatchesGenerate) {
    FlatWorldGenerator gen(32);

    // Generate via grid path
    SimulationGrid grid;
    gen.generate(grid, 0, 0, 0);
    grid.syncChunkBuffers(0, 0, 0);
    // Chunk (0,0,0) is sentinel (all stone), not materialized
    auto fill = grid.getChunkFillValue(0, 0, 0);

    // Generate via buffer path
    std::array<VoxelCell, K_CHUNK_VOLUME> buffer{};
    gen.generateToBuffer(buffer.data(), 0, 0, 0);

    for (size_t i = 0; i < K_CHUNK_VOLUME; ++i) {
        EXPECT_EQ(buffer[i].materialId, fill.materialId) << "index=" << i;
    }
}

TEST(TestWorldGenerator, GenerateToBuffer_LayeredMatchesGenerate) {
    LayeredWorldGenerator gen(28, 4);

    // Grid path
    SimulationGrid grid;
    gen.generate(grid, 0, 0, 0);
    grid.advanceEpoch();

    // Buffer path
    std::array<VoxelCell, K_CHUNK_VOLUME> buffer{};
    gen.generateToBuffer(buffer.data(), 0, 0, 0);

    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
                auto gridCell = grid.readCell(lx, ly, lz);
                EXPECT_EQ(buffer[idx].materialId, gridCell.materialId) << "at (" << lx << "," << ly << "," << lz << ")";
            }
        }
    }
}

TEST(TestWorldGenerator, GenerateToBuffer_NaturalMatchesGenerate) {
    recurse::NoiseGenConfig cfg;
    NaturalWorldGenerator gen(cfg);

    // Grid path
    SimulationGrid grid;
    gen.generate(grid, 0, 0, 0);
    grid.syncChunkBuffers(0, 0, 0);

    // Buffer path
    std::array<VoxelCell, K_CHUNK_VOLUME> buffer{};
    gen.generateToBuffer(buffer.data(), 0, 0, 0);

    const auto* gridBuf = grid.readBuffer(0, 0, 0);
    ASSERT_NE(gridBuf, nullptr);
    for (size_t i = 0; i < K_CHUNK_VOLUME; ++i) {
        EXPECT_EQ(buffer[i].materialId, (*gridBuf)[i].materialId) << "index=" << i;
    }
}

TEST(TestWorldGenerator, BatchGeneration_MatchesSequential) {
    recurse::NoiseGenConfig cfg;
    NaturalWorldGenerator gen(cfg);
    fabric::JobScheduler scheduler(2);

    // Sequential: generate 4 chunks via grid path
    SimulationGrid seqGrid;
    std::tuple<int, int, int> coords[] = {{0, 0, 0}, {1, 0, 0}, {0, -1, 0}, {-1, 0, 1}};
    for (auto [cx, cy, cz] : coords) {
        seqGrid.registry().addChunk(cx, cy, cz);
        seqGrid.materializeChunk(cx, cy, cz);
        auto* buf = seqGrid.writeBuffer(cx, cy, cz);
        ASSERT_NE(buf, nullptr);
        gen.generateToBuffer(buf->data(), cx, cy, cz);
        seqGrid.syncChunkBuffers(cx, cy, cz);
    }

    // Parallel: generate same 4 chunks via parallelFor
    SimulationGrid parGrid;
    struct GenTask {
        VoxelCell* buffer;
        int cx, cy, cz;
    };
    std::vector<GenTask> tasks;
    for (auto [cx, cy, cz] : coords) {
        parGrid.registry().addChunk(cx, cy, cz);
        parGrid.materializeChunk(cx, cy, cz);
        auto* buf = parGrid.writeBuffer(cx, cy, cz);
        ASSERT_NE(buf, nullptr);
        tasks.push_back({buf->data(), cx, cy, cz});
    }
    scheduler.parallelFor(tasks.size(), [&](size_t idx, size_t) {
        gen.generateToBuffer(tasks[idx].buffer, tasks[idx].cx, tasks[idx].cy, tasks[idx].cz);
    });
    for (auto [cx, cy, cz] : coords) {
        parGrid.syncChunkBuffers(cx, cy, cz);
    }

    // Compare all cells
    for (auto [cx, cy, cz] : coords) {
        const auto* seqBuf = seqGrid.readBuffer(cx, cy, cz);
        const auto* parBuf = parGrid.readBuffer(cx, cy, cz);
        ASSERT_NE(seqBuf, nullptr);
        ASSERT_NE(parBuf, nullptr);
        for (size_t i = 0; i < K_CHUNK_VOLUME; ++i) {
            EXPECT_EQ((*parBuf)[i].materialId, (*seqBuf)[i].materialId)
                << "chunk (" << cx << "," << cy << "," << cz << ") index=" << i;
        }
    }
}

TEST(TestWorldGenerator, BatchGeneration_EmptyChunksAboveGround) {
    recurse::NoiseGenConfig cfg;
    cfg.terrainHeight = 16.0f;
    cfg.seaLevel = 8.0f;
    NaturalWorldGenerator gen(cfg);
    fabric::JobScheduler scheduler(2);

    struct GenTask {
        VoxelCell* buffer;
        int cx, cy, cz;
    };

    SimulationGrid grid;
    std::vector<GenTask> tasks;
    for (int cx = 0; cx < 4; ++cx) {
        grid.registry().addChunk(cx, 10, 0);
        grid.materializeChunk(cx, 10, 0);
        auto* buf = grid.writeBuffer(cx, 10, 0);
        ASSERT_NE(buf, nullptr);
        tasks.push_back({buf->data(), cx, 10, 0});
    }
    scheduler.parallelFor(tasks.size(), [&](size_t idx, size_t) {
        gen.generateToBuffer(tasks[idx].buffer, tasks[idx].cx, tasks[idx].cy, tasks[idx].cz);
    });
    for (int cx = 0; cx < 4; ++cx) {
        grid.syncChunkBuffers(cx, 10, 0);
        const auto* buf = grid.readBuffer(cx, 10, 0);
        ASSERT_NE(buf, nullptr);
        bool allAir = true;
        for (size_t i = 0; i < K_CHUNK_VOLUME; ++i) {
            if (isOccupied((*buf)[i])) {
                allAir = false;
                break;
            }
        }
        EXPECT_TRUE(allAir) << "Chunk at y=10 should be all air";
    }
}

#include "fabric/core/CompilerHints.hh"
#include "fabric/fx/WorldContext.hh"
#include "recurse/persistence/WorldSession.hh"
#include "recurse/systems/MainMenuSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/ChunkOps.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/resource/AssetRegistry.hh"
#include "fabric/resource/ResourceHub.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <gtest/gtest.h>

#include <flecs.h>

namespace fs = std::filesystem;

using namespace recurse::systems;
using recurse::simulation::cellForMaterial;
using recurse::simulation::ChunkSlotState;
using recurse::simulation::VoxelCell;
using recurse::simulation::material_ids::STONE;

// Noinline wrappers isolate each call chain for assembly comparison.
// With FABRIC_ALWAYS_INLINE on WorldContext::resolve, readViaResolve
// should produce identical object code to readDirect in Release builds.
//
// Verify:
//   objdump -d build/release/tests/UnitTests | grep -A 30 'readDirect\|readViaResolve'

FABRIC_NOINLINE const VoxelCell* readDirect(recurse::WorldSession& session, int cx, int cy, int cz) {
    return session.resolve(recurse::ops::ReadBuffer{cx, cy, cz});
}

FABRIC_NOINLINE const VoxelCell* readViaResolve(fabric::fx::WorldContext<recurse::WorldSession>& ctx, int cx, int cy,
                                                int cz) {
    return ctx.resolve(recurse::ops::ReadBuffer{cx, cy, cz});
}

class WorldContextBenchmark : public ::testing::Test {
  protected:
    fabric::World world;
    fabric::Timeline timeline;
    fabric::EventDispatcher dispatcher;
    fabric::ResourceHub hub;
    fabric::AssetRegistry assetRegistry{hub};
    fabric::SystemRegistry systemRegistry;
    fabric::ConfigManager configManager;
    fabric::JobScheduler scheduler;

    VoxelSimulationSystem* voxelSim_ = nullptr;
    std::unique_ptr<recurse::WorldSession> session_;
    fs::path tmpDir_;

    void SetUp() override {
        hub.disableWorkerThreadsForTesting();
        scheduler.disableForTesting();

        fabric::AppContext ctx = makeCtx();
        auto& vs = systemRegistry.registerSystem<VoxelSimulationSystem>(fabric::SystemPhase::FixedUpdate);
        voxelSim_ = &vs;
        systemRegistry.resolve();
        voxelSim_->init(ctx);

        materializeActiveChunk(0, 0, 0);

        tmpDir_ = fs::temp_directory_path() /
                  ("fabric_ctx_bench_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(tmpDir_);

        auto result = recurse::WorldSession::open(tmpDir_.string(), dispatcher, scheduler, world.get(), voxelSim_,
                                                  nullptr, nullptr, nullptr, nullptr);
        ASSERT_TRUE(result.isSuccess());
        session_ = std::move(result).value();
    }

    void TearDown() override {
        session_.reset();
        voxelSim_->shutdown();
        fs::remove_all(tmpDir_);
    }

    fabric::AppContext makeCtx() {
        return fabric::AppContext{
            .world = world,
            .timeline = timeline,
            .dispatcher = dispatcher,
            .resourceHub = hub,
            .assetRegistry = assetRegistry,
            .systemRegistry = systemRegistry,
            .configManager = configManager,
        };
    }

    void materializeActiveChunk(int cx, int cy, int cz) {
        using namespace recurse::simulation;
        auto& grid = voxelSim_->simulationGrid();
        auto& reg = grid.registry();
        auto absent = addChunkRef(reg, cx, cy, cz);
        auto generating = transition<Absent, Generating>(absent, reg);
        grid.materializeChunk(cx, cy, cz);
        grid.writeCell(cx * 32 + 4, cy * 32 + 4, cz * 32 + 4, cellForMaterial(STONE));
        grid.syncChunkBuffers(cx, cy, cz);
        transition<Generating, Active>(generating, reg);
    }
};

// Both paths should return identical pointers to the same underlying buffer.
TEST_F(WorldContextBenchmark, ResolveMatchesDirect) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);

    const VoxelCell* direct = readDirect(*session_, 0, 0, 0);
    const VoxelCell* resolved = readViaResolve(ctx, 0, 0, 0);

    ASSERT_NE(direct, nullptr);
    EXPECT_EQ(direct, resolved);
}

// Timing microbenchmark. Debug builds won't inline so expect wider delta.
// Release builds should show <1% overhead.
TEST_F(WorldContextBenchmark, ResolveZeroOverhead) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);

    constexpr int K_ITERATIONS = 100'000;
    volatile const VoxelCell* sink = nullptr;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < K_ITERATIONS; ++i)
        sink = readDirect(*session_, 0, 0, 0);
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < K_ITERATIONS; ++i)
        sink = readViaResolve(ctx, 0, 0, 0);
    auto t2 = std::chrono::high_resolution_clock::now();

    (void)sink;

    auto directNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    auto resolveNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

    std::printf("  direct:  %lld ns (%lld ns/call)\n", static_cast<long long>(directNs),
                static_cast<long long>(directNs / K_ITERATIONS));
    std::printf("  resolve: %lld ns (%lld ns/call)\n", static_cast<long long>(resolveNs),
                static_cast<long long>(resolveNs / K_ITERATIONS));

    // 10% tolerance covers Debug overhead where always_inline has no effect.
    // Release builds with objdump verification should show identical assembly.
    double ratio = static_cast<double>(resolveNs) / static_cast<double>(directNs);
    EXPECT_LT(ratio, 1.10) << "resolve path >10% slower than direct";
}

TEST(ProfileBenchmarkRouteTest, SampleStartsAtPresetSpawn) {
    const auto preset = makeProfileBenchmarkPreset(4242);
    const auto sample = sampleProfileBenchmarkMotion(preset, 0, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(sample.position.x, preset.spawnPosition.x);
    EXPECT_FLOAT_EQ(sample.position.y, preset.spawnPosition.y);
    EXPECT_FLOAT_EQ(sample.position.z, preset.spawnPosition.z);
    EXPECT_FLOAT_EQ(sample.yawDegrees, 90.0f);
    EXPECT_FLOAT_EQ(sample.pitchDegrees, preset.pitchDegrees);
    EXPECT_FALSE(sample.complete);
}

TEST(ProfileBenchmarkRouteTest, SampleCompletesAtDeterministicEndpoint) {
    const auto preset = makeProfileBenchmarkPreset(4242);
    const auto sample = sampleProfileBenchmarkMotion(preset, totalProfileBenchmarkMotionTicks() + 240, 1.0f / 60.0f);

    EXPECT_NEAR(sample.position.x, preset.spawnPosition.x + 144.0f, 0.001f);
    EXPECT_NEAR(sample.position.z, preset.spawnPosition.z + 144.0f, 0.001f);
    EXPECT_FLOAT_EQ(sample.position.y, preset.spawnPosition.y);
    EXPECT_TRUE(sample.complete);
}

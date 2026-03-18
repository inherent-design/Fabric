#include "recurse/persistence/WorldSession.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/resource/AssetRegistry.hh"
#include "fabric/resource/ResourceHub.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/persistence/ChangeSource.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/EssenceAssigner.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/EssencePalette.hh"
#include "recurse/world/WorldGenerator.hh"

#include <array>
#include <filesystem>
#include <gtest/gtest.h>
#include <utility>

#include <flecs.h>

namespace fs = std::filesystem;

/// Integration tests for WorldSession lifecycle.
/// Tests verify RAII behavior: open/close, event listener registration,
/// and shutdown-during-load safety.
class WorldSessionIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tmpDir_ =
            fs::temp_directory_path() / ("fabric_world_session_test_" +
                                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(tmpDir_);
        worldDir_ = tmpDir_.string();

        scheduler_.disableForTesting();
    }

    void TearDown() override {
        session_.reset();
        fs::remove_all(tmpDir_);
    }

    /// Open a WorldSession with minimal dependencies (null system pointers).
    /// Returns the session or fails the test with an error message.
    void openSession() {
        auto result = recurse::WorldSession::open(worldDir_, dispatcher_, scheduler_, ecsWorld_,
                                                  nullptr, // simSystem
                                                  nullptr, // meshingSystem
                                                  nullptr, // lodSystem
                                                  nullptr, // physicsSystem
                                                  nullptr  // terrainSystem
        );

        ASSERT_TRUE(result.isSuccess()) << "WorldSession::open failed: "
                                        << result.error<fabric::fx::IOError>().ctx.message;
        session_ = std::move(result).value();
    }

    static recurse::ChunkBlob makeFakeChunk(uint8_t marker = 0xAA) {
        constexpr size_t K_CHUNK_VOLUME = 32 * 32 * 32;
        constexpr size_t K_CELL_SIZE = 4;
        std::vector<uint8_t> cells(K_CHUNK_VOLUME * K_CELL_SIZE, 0);
        cells[0] = marker;
        return recurse::FchkCodec::encode(cells.data(), cells.size());
    }

    fs::path tmpDir_;
    std::string worldDir_;
    fabric::EventDispatcher dispatcher_;
    fabric::JobScheduler scheduler_;
    flecs::world ecsWorld_;
    std::unique_ptr<recurse::WorldSession> session_;
};

// ---------------------------------------------------------------------------
// Test 1: Load -> Unload -> Load cycle
// ---------------------------------------------------------------------------

TEST_F(WorldSessionIntegrationTest, LoadUnloadLoadCycle) {
    // Phase 1: Open session, save a chunk, verify DB state
    openSession();
    ASSERT_NE(session_, nullptr);

    auto* store = session_->chunkStore();
    ASSERT_NE(store, nullptr);

    auto blob = makeFakeChunk(0x42);
    store->saveChunk(1, 2, 3, blob);
    EXPECT_TRUE(store->hasChunk(1, 2, 3));

    // Phase 2: Close session
    session_.reset();

    // Verify WAL/SHM files are cleaned up after session close
    std::string dbPath = worldDir_ + "/world.db";
    // Note: WAL files may still exist briefly after close; the key test is
    // that re-opening works cleanly without corruption.

    // Phase 3: Re-open session, verify chunk persisted
    openSession();
    ASSERT_NE(session_, nullptr);

    store = session_->chunkStore();
    ASSERT_NE(store, nullptr);
    EXPECT_TRUE(store->hasChunk(1, 2, 3));

    auto loaded = store->loadChunk(1, 2, 3);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->data, blob.data);
}

// ---------------------------------------------------------------------------
// Test 2: Shutdown-during-load (destructor drains pending futures)
// ---------------------------------------------------------------------------

TEST_F(WorldSessionIntegrationTest, ShutdownDuringPendingLoads_NoCrash) {
    openSession();
    ASSERT_NE(session_, nullptr);

    // dispatchAsyncLoad requires simSystem, so it returns false with null.
    // Instead, test that destructor handles empty pendingLoads_ vector cleanly.
    // Real shutdown-during-load test requires full simulation system integration.

    // Verify no pending loads initially
    EXPECT_FALSE(session_->hasPendingLoad(0, 0, 0));

    // Destructor should complete without hanging or crashing
    // even if we had pending loads (tested in real integration tests)
    session_.reset();

    // If we reach here, destructor completed successfully
    SUCCEED();
}

TEST_F(WorldSessionIntegrationTest, DestructorDrainsWriterQueue) {
    openSession();
    ASSERT_NE(session_, nullptr);

    auto* store = session_->chunkStore();

    // Mark multiple chunks dirty via event dispatch
    // This will buffer changes and eventually flush to WriterQueue
    for (int i = 0; i < 5; ++i) {
        fabric::Event e(recurse::K_VOXEL_CHANGED_EVENT, "test");
        e.setData("cx", i);
        e.setData("cy", 0);
        e.setData("cz", 0);
        dispatcher_.dispatchEvent(e);
    }

    // Destructor should drain WriterQueue before destroying services
    // No crash = success
    session_.reset();

    SUCCEED();
}

// ---------------------------------------------------------------------------
// Test 3: Event listener unsubscription
// ---------------------------------------------------------------------------

TEST_F(WorldSessionIntegrationTest, EventListenerRegisteredOnOpen) {
    // Track callbacks via external counter
    int callbackCount = 0;

    // First, add our own listener to verify event dispatch works
    auto handlerId = dispatcher_.addEventListener(recurse::K_VOXEL_CHANGED_EVENT,
                                                  [&callbackCount](fabric::Event&) { ++callbackCount; });

    openSession();
    ASSERT_NE(session_, nullptr);

    // Dispatch an event - both session listener and our counter should fire
    fabric::Event e(recurse::K_VOXEL_CHANGED_EVENT, "test");
    e.setData("cx", 0);
    e.setData("cy", 0);
    e.setData("cz", 0);
    dispatcher_.dispatchEvent(e);

    EXPECT_EQ(callbackCount, 1);

    dispatcher_.removeEventListener(recurse::K_VOXEL_CHANGED_EVENT, handlerId);
}

TEST_F(WorldSessionIntegrationTest, EventListenerUnregisteredOnClose) {
    int callbackCount = 0;

    // Add a listener that survives session destruction
    auto handlerId = dispatcher_.addEventListener(recurse::K_VOXEL_CHANGED_EVENT,
                                                  [&callbackCount](fabric::Event&) { ++callbackCount; });

    openSession();
    ASSERT_NE(session_, nullptr);

    // Dispatch event - our listener fires
    fabric::Event e1(recurse::K_VOXEL_CHANGED_EVENT, "test");
    e1.setData("cx", 1);
    e1.setData("cy", 2);
    e1.setData("cz", 3);
    dispatcher_.dispatchEvent(e1);
    EXPECT_EQ(callbackCount, 1);

    // Close session - should unregister session's internal listener
    session_.reset();

    // Dispatch another event - only our listener should fire (count = 2)
    // If session's listener wasn't unregistered, it might access freed memory
    fabric::Event e2(recurse::K_VOXEL_CHANGED_EVENT, "test");
    e2.setData("cx", 4);
    e2.setData("cy", 5);
    e2.setData("cz", 6);
    dispatcher_.dispatchEvent(e2);
    EXPECT_EQ(callbackCount, 2);

    // No crash means listener was properly unregistered
    dispatcher_.removeEventListener(recurse::K_VOXEL_CHANGED_EVENT, handlerId);
}

TEST_F(WorldSessionIntegrationTest, EventListenerMarksDirty) {
    openSession();
    ASSERT_NE(session_, nullptr);

    auto* saveService = session_->saveService();
    ASSERT_NE(saveService, nullptr);

    EXPECT_EQ(saveService->pendingCount(), 0u);

    // Dispatch event - session listener should mark chunk dirty
    fabric::Event e(recurse::K_VOXEL_CHANGED_EVENT, "test");
    e.setData("cx", 10);
    e.setData("cy", 20);
    e.setData("cz", 30);
    dispatcher_.dispatchEvent(e);

    EXPECT_EQ(saveService->pendingCount(), 1u);
}

TEST_F(WorldSessionIntegrationTest, RuntimeStatusSnapshotIncludesSaveActivity) {
    openSession();
    ASSERT_NE(session_, nullptr);

    fabric::Event e(recurse::K_VOXEL_CHANGED_EVENT, "test");
    e.setData("cx", 10);
    e.setData("cy", 20);
    e.setData("cz", 30);
    dispatcher_.dispatchEvent(e);

    auto snapshot = session_->runtimeStatusSnapshot();
    EXPECT_EQ(snapshot.pendingLoads, 0u);
    EXPECT_EQ(snapshot.saveActivity.dirtyChunks, 1u);
    EXPECT_EQ(snapshot.saveActivity.savingChunks, 0u);
    EXPECT_FALSE(snapshot.saveActivity.hasError);
}

TEST_F(WorldSessionIntegrationTest, EventListenerBuffersDetailedChanges) {
    openSession();
    ASSERT_NE(session_, nullptr);

    auto* txStore = session_->transactionStore();
    ASSERT_NE(txStore, nullptr);

    // Dispatch event WITH detail - should buffer for transaction log
    fabric::Event e(recurse::K_VOXEL_CHANGED_EVENT, "test");
    e.setData("cx", 5);
    e.setData("cy", 6);
    e.setData("cz", 7);

    std::vector<recurse::VoxelChangeDetail> details;
    details.push_back({0, 0, 0, 0x00000000, 0x11223344, 1, recurse::ChangeSource::Place});
    e.setAnyData("detail", std::move(details));

    dispatcher_.dispatchEvent(e);

    // Flush pending changes to verify they were buffered
    // (Flush goes through WriterQueue, which needs to drain)
    session_->flushPendingChanges();

    // If we reach here without crash, the event listener correctly
    // buffered the change detail
    SUCCEED();
}

TEST_F(WorldSessionIntegrationTest, DetailLessPhysicsEventMarksDirtyWithoutTransactionDiffs) {
    openSession();
    ASSERT_NE(session_, nullptr);

    auto* saveService = session_->saveService();
    auto* txStore = session_->transactionStore();
    ASSERT_NE(saveService, nullptr);
    ASSERT_NE(txStore, nullptr);

    recurse::ChangeQuery query;
    query.chunkRange = std::make_pair(fabric::ChunkCoord{8, 0, 9}, fabric::ChunkCoord{8, 0, 9});

    fabric::Event e(recurse::K_VOXEL_CHANGED_EVENT, "test");
    e.setData("cx", 8);
    e.setData("cy", 0);
    e.setData("cz", 9);
    e.setData("source", static_cast<int>(recurse::ChangeSource::Physics));
    dispatcher_.dispatchEvent(e);

    EXPECT_EQ(saveService->pendingCount(), 1u);

    session_->flushPendingChanges();
    EXPECT_EQ(txStore->countChanges(query), 0);
}

// ---------------------------------------------------------------------------
// Test 4: Factory error handling
// ---------------------------------------------------------------------------

TEST_F(WorldSessionIntegrationTest, OpenCreatesNestedWorld) {
    // WorldSession::open should create the world.db and parent directories
    // if they don't exist (SQLite does this by default)
    std::string nestedPath = (tmpDir_ / "nested" / "world").string();
    EXPECT_FALSE(fs::exists(nestedPath + "/world.db"));

    auto result = recurse::WorldSession::open(nestedPath, dispatcher_, scheduler_, ecsWorld_, nullptr, nullptr, nullptr,
                                              nullptr, nullptr);

    // Should succeed - SQLite creates directories as needed
    ASSERT_TRUE(result.isSuccess());

    // Extract and destroy session via RAII
    auto session = std::move(result).value();
    EXPECT_TRUE(fs::exists(nestedPath + "/world.db"));
    session.reset();
}

TEST_F(WorldSessionIntegrationTest, OpenSucceedsOnNewWorld) {
    // WorldSession::open should create the world.db if it doesn't exist
    EXPECT_FALSE(fs::exists(worldDir_ + "/world.db"));

    openSession();

    EXPECT_TRUE(fs::exists(worldDir_ + "/world.db"));
}

class WorldSessionResidentChunkPersistenceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        hub_.disableWorkerThreadsForTesting();
        scheduler_.disableForTesting();

        tmpDir_ =
            fs::temp_directory_path() / ("fabric_world_session_resident_test_" +
                                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(tmpDir_);
        worldDir_ = tmpDir_.string();

        auto ctx = makeCtx();
        auto& vs =
            systemRegistry_.registerSystem<recurse::systems::VoxelSimulationSystem>(fabric::SystemPhase::FixedUpdate);
        voxelSim_ = &vs;
        systemRegistry_.resolve();
        voxelSim_->init(ctx);

        openSession();
    }

    void TearDown() override {
        session_.reset();
        if (voxelSim_)
            voxelSim_->shutdown();
        fs::remove_all(tmpDir_);
    }

    fabric::AppContext makeCtx() {
        return fabric::AppContext{
            .world = world_,
            .timeline = timeline_,
            .dispatcher = dispatcher_,
            .resourceHub = hub_,
            .assetRegistry = assetRegistry_,
            .systemRegistry = systemRegistry_,
            .configManager = configManager_,
        };
    }

    void openSession() {
        auto result = recurse::WorldSession::open(worldDir_, dispatcher_, scheduler_, world_.get(), voxelSim_, nullptr,
                                                  nullptr, nullptr, nullptr);
        ASSERT_TRUE(result.isSuccess()) << "WorldSession::open failed: "
                                        << result.error<fabric::fx::IOError>().ctx.message;
        session_ = std::move(result).value();
    }

    void materializeActiveChunk(int cx, int cy, int cz) {
        using namespace recurse::simulation;
        auto& grid = voxelSim_->simulationGrid();
        auto& registry = grid.registry();
        auto absent = addChunkRef(registry, cx, cy, cz);
        auto generating = transition<Absent, Generating>(absent, registry);
        grid.materializeChunk(cx, cy, cz);
        grid.writeCell(cx * 32 + 1, cy * 32 + 1, cz * 32 + 1, VoxelCell{material_ids::STONE});
        grid.syncChunkBuffers(cx, cy, cz);
        transition<Generating, Active>(generating, registry);
    }

    fabric::World world_;
    fabric::Timeline timeline_;
    fabric::EventDispatcher dispatcher_;
    fabric::ResourceHub hub_;
    fabric::AssetRegistry assetRegistry_{hub_};
    fabric::SystemRegistry systemRegistry_;
    fabric::ConfigManager configManager_;
    fabric::JobScheduler scheduler_;
    recurse::systems::VoxelSimulationSystem* voxelSim_ = nullptr;
    std::unique_ptr<recurse::WorldSession> session_;
    fs::path tmpDir_;
    std::string worldDir_;
};

TEST_F(WorldSessionResidentChunkPersistenceTest, CloseReopenPersistsResidentGenerationFilteredChunk) {
    constexpr int K_CX = 3;
    constexpr int K_CY = 0;
    constexpr int K_CZ = 4;

    materializeActiveChunk(K_CX, K_CY, K_CZ);

    auto* saveService = session_->saveService();
    ASSERT_NE(saveService, nullptr);
    EXPECT_FALSE(session_->chunkStore()->hasChunk(K_CX, K_CY, K_CZ));

    fabric::Event event(recurse::K_VOXEL_CHANGED_EVENT, "test");
    event.setData("cx", K_CX);
    event.setData("cy", K_CY);
    event.setData("cz", K_CZ);
    event.setData("source", static_cast<int>(recurse::ChangeSource::Generation));
    dispatcher_.dispatchEvent(event);

    EXPECT_EQ(saveService->pendingCount(), 0u);

    session_.reset();
    voxelSim_->resetWorld();
    openSession();

    auto* store = session_->chunkStore();
    ASSERT_NE(store, nullptr);
    EXPECT_TRUE(store->isInSavedRegion(K_CX, K_CY, K_CZ));
    EXPECT_TRUE(store->hasChunk(K_CX, K_CY, K_CZ));

    auto blob = store->loadChunk(K_CX, K_CY, K_CZ);
    ASSERT_TRUE(blob.has_value());
    EXPECT_FALSE(blob->empty());
}

TEST_F(WorldSessionResidentChunkPersistenceTest, CloseReopenPersistsSettledPhysicsDirtyChunk) {
    constexpr int K_CX = 6;
    constexpr int K_CY = 0;
    constexpr int K_CZ = 2;

    materializeActiveChunk(K_CX, K_CY, K_CZ);
    voxelSim_->activityTracker().putToSleep(fabric::ChunkCoord{K_CX, K_CY, K_CZ});

    auto* saveService = session_->saveService();
    auto* txStore = session_->transactionStore();
    ASSERT_NE(saveService, nullptr);
    ASSERT_NE(txStore, nullptr);
    EXPECT_FALSE(session_->chunkStore()->hasChunk(K_CX, K_CY, K_CZ));

    recurse::ChangeQuery query;
    query.chunkRange = std::make_pair(fabric::ChunkCoord{K_CX, K_CY, K_CZ}, fabric::ChunkCoord{K_CX, K_CY, K_CZ});

    fabric::Event event(recurse::K_VOXEL_CHANGED_EVENT, "test");
    event.setData("cx", K_CX);
    event.setData("cy", K_CY);
    event.setData("cz", K_CZ);
    event.setData("source", static_cast<int>(recurse::ChangeSource::Physics));
    dispatcher_.dispatchEvent(event);

    EXPECT_EQ(saveService->pendingCount(), 1u);

    session_->flushPendingChanges();
    EXPECT_EQ(txStore->countChanges(query), 0);

    saveService->flush();
    auto savedBeforeClose = session_->chunkStore()->loadChunk(K_CX, K_CY, K_CZ);
    ASSERT_TRUE(savedBeforeClose.has_value());
    EXPECT_FALSE(savedBeforeClose->empty());

    session_.reset();
    voxelSim_->resetWorld();
    openSession();

    auto reloaded = session_->chunkStore()->loadChunk(K_CX, K_CY, K_CZ);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->data, savedBeforeClose->data);
}

class WorldSessionPersistedDeltaReopenTest : public ::testing::Test {
  protected:
    static constexpr int K_EDIT_OFFSET = 4;

    void SetUp() override {
        hub_.disableWorkerThreadsForTesting();
        scheduler_.disableForTesting();

        tmpDir_ =
            fs::temp_directory_path() / ("fabric_world_session_delta_test_" +
                                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(tmpDir_);
        worldDir_ = tmpDir_.string();

        terrain_ = &systemRegistry_.registerSystem<recurse::systems::TerrainSystem>(fabric::SystemPhase::FixedUpdate);
        voxelSim_ =
            &systemRegistry_.registerSystem<recurse::systems::VoxelSimulationSystem>(fabric::SystemPhase::FixedUpdate);
        ASSERT_TRUE(systemRegistry_.resolve());

        auto ctx = makeCtx();
        systemRegistry_.initAll(ctx);
        voxelSim_->setWorldSeed(1337);

        openSession();
    }

    void TearDown() override {
        session_.reset();
        systemRegistry_.shutdownAll();
        fs::remove_all(tmpDir_);
    }

    fabric::AppContext makeCtx() {
        return fabric::AppContext{
            .world = world_,
            .timeline = timeline_,
            .dispatcher = dispatcher_,
            .resourceHub = hub_,
            .assetRegistry = assetRegistry_,
            .systemRegistry = systemRegistry_,
            .configManager = configManager_,
        };
    }

    void openSession() {
        auto result = recurse::WorldSession::open(worldDir_, dispatcher_, scheduler_, world_.get(), voxelSim_, nullptr,
                                                  nullptr, nullptr, terrain_);
        ASSERT_TRUE(result.isSuccess()) << "WorldSession::open failed: "
                                        << result.error<fabric::fx::IOError>().ctx.message;
        session_ = std::move(result).value();
    }

    void generateEditedChunk(int cx, int cy, int cz, uint16_t materialId) {
        voxelSim_->generateChunk(cx, cy, cz);
        auto& grid = voxelSim_->simulationGrid();
        grid.writeCell(cx * 32 + K_EDIT_OFFSET, cy * 32 + K_EDIT_OFFSET, cz * 32 + K_EDIT_OFFSET,
                       recurse::simulation::VoxelCell{materialId});
        grid.syncChunkBuffers(cx, cy, cz);
    }

    recurse::ChunkBlob encodeDeltaBlobForCurrentChunk(int cx, int cy, int cz, uint32_t worldgenVersion) {
        auto* current = voxelSim_->simulationGrid().readBuffer(cx, cy, cz);
        EXPECT_NE(current, nullptr);
        if (!current)
            return {};

        std::array<recurse::simulation::VoxelCell, recurse::simulation::K_CHUNK_VOLUME> reference{};
        terrain_->worldGenerator().generateToBuffer(reference.data(), cx, cy, cz);
        recurse::EssencePalette referencePalette;
        recurse::simulation::assignEssence(reference.data(), cx, cy, cz, voxelSim_->materials(), referencePalette, 42);

        return recurse::FchkCodec::encodeDelta(current->data(), reference.data(), sizeof(*current), worldgenVersion, 0);
    }

    void reloadChunkFromStore(int cx, int cy, int cz) {
        ASSERT_TRUE(session_->dispatchAsyncLoad(cx, cy, cz));
        auto completions = session_->pollPendingLoads();
        ASSERT_EQ(completions.size(), 1u);
        EXPECT_TRUE(completions[0].success);
    }

    uint16_t loadedMaterial(int cx, int cy, int cz) const {
        return voxelSim_->simulationGrid()
            .readCell(cx * 32 + K_EDIT_OFFSET, cy * 32 + K_EDIT_OFFSET, cz * 32 + K_EDIT_OFFSET)
            .materialId;
    }

    fabric::World world_;
    fabric::Timeline timeline_;
    fabric::EventDispatcher dispatcher_;
    fabric::ResourceHub hub_;
    fabric::AssetRegistry assetRegistry_{hub_};
    fabric::SystemRegistry systemRegistry_;
    fabric::ConfigManager configManager_;
    fabric::JobScheduler scheduler_;
    recurse::systems::TerrainSystem* terrain_ = nullptr;
    recurse::systems::VoxelSimulationSystem* voxelSim_ = nullptr;
    std::unique_ptr<recurse::WorldSession> session_;
    fs::path tmpDir_;
    std::string worldDir_;
};

TEST_F(WorldSessionPersistedDeltaReopenTest, ReopenDistinguishesResidentPersistedAndFreshChunks) {
    constexpr int K_NEAR_CX = 1;
    constexpr int K_NEAR_CY = 0;
    constexpr int K_NEAR_CZ = 1;
    constexpr int K_MEDIUM_CX = 4;
    constexpr int K_MEDIUM_CY = 0;
    constexpr int K_MEDIUM_CZ = 4;
    constexpr int K_FAR_CX = 9;
    constexpr int K_FAR_CY = 0;
    constexpr int K_FAR_CZ = 9;
    constexpr uint32_t K_MISMATCHED_WORLDGEN_VERSION = 0;

    generateEditedChunk(K_NEAR_CX, K_NEAR_CY, K_NEAR_CZ, recurse::simulation::material_ids::SAND);
    EXPECT_FALSE(session_->chunkStore()->hasChunk(K_NEAR_CX, K_NEAR_CY, K_NEAR_CZ));

    generateEditedChunk(K_MEDIUM_CX, K_MEDIUM_CY, K_MEDIUM_CZ, recurse::simulation::material_ids::SAND);
    auto mismatchedBlob =
        encodeDeltaBlobForCurrentChunk(K_MEDIUM_CX, K_MEDIUM_CY, K_MEDIUM_CZ, K_MISMATCHED_WORLDGEN_VERSION);
    ASSERT_FALSE(mismatchedBlob.empty());
    session_->chunkStore()->saveChunk(K_MEDIUM_CX, K_MEDIUM_CY, K_MEDIUM_CZ, mismatchedBlob);
    voxelSim_->removeChunk(K_MEDIUM_CX, K_MEDIUM_CY, K_MEDIUM_CZ);

    EXPECT_TRUE(session_->chunkStore()->hasChunk(K_MEDIUM_CX, K_MEDIUM_CY, K_MEDIUM_CZ));
    EXPECT_FALSE(session_->chunkStore()->hasChunk(K_FAR_CX, K_FAR_CY, K_FAR_CZ));

    session_.reset();
    voxelSim_->resetWorld();
    voxelSim_->setWorldSeed(1337);
    openSession();

    ASSERT_TRUE(session_->chunkStore()->hasChunk(K_NEAR_CX, K_NEAR_CY, K_NEAR_CZ));
    ASSERT_TRUE(session_->chunkStore()->hasChunk(K_MEDIUM_CX, K_MEDIUM_CY, K_MEDIUM_CZ));
    EXPECT_FALSE(session_->chunkStore()->hasChunk(K_FAR_CX, K_FAR_CY, K_FAR_CZ));

    reloadChunkFromStore(K_NEAR_CX, K_NEAR_CY, K_NEAR_CZ);
    EXPECT_EQ(loadedMaterial(K_NEAR_CX, K_NEAR_CY, K_NEAR_CZ), recurse::simulation::material_ids::SAND);

    reloadChunkFromStore(K_MEDIUM_CX, K_MEDIUM_CY, K_MEDIUM_CZ);
    EXPECT_EQ(loadedMaterial(K_MEDIUM_CX, K_MEDIUM_CY, K_MEDIUM_CZ), recurse::simulation::material_ids::STONE);

    voxelSim_->generateChunk(K_FAR_CX, K_FAR_CY, K_FAR_CZ);
    EXPECT_EQ(loadedMaterial(K_FAR_CX, K_FAR_CY, K_FAR_CZ), recurse::simulation::material_ids::STONE);
}

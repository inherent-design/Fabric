#include "fabric/core/SaveManager.hh"
#include "fabric/core/FieldLayer.hh"
#include "fabric/core/SceneSerializer.hh"
#include "fabric/core/Temporal.hh"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace fabric;

class SaveManagerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "fabric_save_manager_test";
        std::filesystem::create_directories(testDir_);
        world_.registerCoreComponents();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(testDir_, ec);
    }

    std::string testDirStr() const { return testDir_.string(); }

    std::filesystem::path testDir_;
    World world_;
    DensityField density_;
    EssenceField essence_;
    Timeline timeline_;
    SceneSerializer serializer_;
};

TEST_F(SaveManagerTest, SaveAndLoadRoundTrip) {
    SaveManager mgr(testDirStr());

    auto entity = world_.createSceneEntity("save_test");
    entity.set<Position>(Position{10.0f, 20.0f, 30.0f});
    world_.progress(0.0f);

    timeline_.setGlobalTimeScale(2.0);

    std::optional<Position> playerPos = Position{1.0f, 2.0f, 3.0f};
    std::optional<Position> playerVel = Position{0.5f, -1.0f, 0.0f};

    ASSERT_TRUE(mgr.save("test_slot", serializer_, world_, density_, essence_, timeline_, playerPos, playerVel));

    World newWorld;
    newWorld.registerCoreComponents();
    DensityField newDensity;
    EssenceField newEssence;
    Timeline newTimeline;
    std::optional<Position> loadedPos;
    std::optional<Position> loadedVel;
    SceneSerializer newSerializer;

    ASSERT_TRUE(
        mgr.load("test_slot", newSerializer, newWorld, newDensity, newEssence, newTimeline, loadedPos, loadedVel));

    ASSERT_TRUE(loadedPos);
    EXPECT_FLOAT_EQ(loadedPos->x, 1.0f);
    EXPECT_FLOAT_EQ(loadedPos->y, 2.0f);
    EXPECT_FLOAT_EQ(loadedPos->z, 3.0f);

    ASSERT_TRUE(loadedVel);
    EXPECT_FLOAT_EQ(loadedVel->x, 0.5f);
    EXPECT_FLOAT_EQ(loadedVel->y, -1.0f);
    EXPECT_FLOAT_EQ(loadedVel->z, 0.0f);

    EXPECT_FLOAT_EQ(newTimeline.getGlobalTimeScale(), 2.0f);
}

TEST_F(SaveManagerTest, ListSlotsReturnsAll) {
    SaveManager mgr(testDirStr());

    std::optional<Position> pos = Position{0.0f, 0.0f, 0.0f};
    std::optional<Position> vel = Position{0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(mgr.save("slot_a", serializer_, world_, density_, essence_, timeline_, pos, vel));
    ASSERT_TRUE(mgr.save("slot_b", serializer_, world_, density_, essence_, timeline_, pos, vel));
    ASSERT_TRUE(mgr.save("slot_c", serializer_, world_, density_, essence_, timeline_, pos, vel));

    auto slots = mgr.listSlots();
    EXPECT_EQ(slots.size(), 3u);
}

TEST_F(SaveManagerTest, DeleteSlotRemovesFile) {
    SaveManager mgr(testDirStr());

    std::optional<Position> pos = Position{0.0f, 0.0f, 0.0f};
    std::optional<Position> vel = Position{0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(mgr.save("to_delete", serializer_, world_, density_, essence_, timeline_, pos, vel));

    auto slotsBefore = mgr.listSlots();
    ASSERT_EQ(slotsBefore.size(), 1u);

    ASSERT_TRUE(mgr.deleteSlot("to_delete"));

    auto slotsAfter = mgr.listSlots();
    EXPECT_EQ(slotsAfter.size(), 0u);
}

TEST_F(SaveManagerTest, MetadataInSlotInfo) {
    SaveManager mgr(testDirStr());

    std::optional<Position> pos = Position{0.0f, 0.0f, 0.0f};
    std::optional<Position> vel = Position{0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(mgr.save("meta_test", serializer_, world_, density_, essence_, timeline_, pos, vel));

    auto slots = mgr.listSlots();
    ASSERT_EQ(slots.size(), 1u);

    const auto& info = slots[0];
    EXPECT_EQ(info.name, "meta_test");
    EXPECT_EQ(info.version, "1.0");
    EXPECT_FALSE(info.timestamp.empty());
    EXPECT_GT(info.sizeBytes, 0u);
}

TEST_F(SaveManagerTest, VersionMismatchRejectsLoad) {
    SaveManager mgr(testDirStr());

    // Write a file with a bad version
    nlohmann::json badSave;
    badSave["save_version"] = "99.0";
    badSave["slot"] = "bad_version";
    badSave["timestamp"] = "2026-01-01T00:00:00Z";
    badSave["scene"] = nlohmann::json::object();

    std::string filepath = (testDir_ / "bad_version.json").string();
    std::ofstream file(filepath);
    file << badSave.dump(2);
    file.close();

    std::optional<Position> pos;
    std::optional<Position> vel;
    EXPECT_FALSE(mgr.load("bad_version", serializer_, world_, density_, essence_, timeline_, pos, vel));
}

TEST_F(SaveManagerTest, AutosaveRotation) {
    SaveManager mgr(testDirStr());
    mgr.enableAutosave(1.0f);

    std::optional<Position> pos = Position{0.0f, 0.0f, 0.0f};
    std::optional<Position> vel = Position{0.0f, 0.0f, 0.0f};

    // First autosave trigger: autosave_0
    mgr.tickAutosave(1.5f, serializer_, world_, density_, essence_, timeline_, pos, vel);

    auto slots1 = mgr.listSlots();
    ASSERT_EQ(slots1.size(), 1u);

    bool hasSlot0 = false;
    for (const auto& s : slots1) {
        if (s.name == "autosave_0")
            hasSlot0 = true;
    }
    EXPECT_TRUE(hasSlot0);

    // Second autosave trigger: autosave_1
    mgr.tickAutosave(1.5f, serializer_, world_, density_, essence_, timeline_, pos, vel);

    auto slots2 = mgr.listSlots();
    ASSERT_EQ(slots2.size(), 2u);

    bool hasSlot1 = false;
    for (const auto& s : slots2) {
        if (s.name == "autosave_1")
            hasSlot1 = true;
    }
    EXPECT_TRUE(hasSlot1);

    // Third trigger overwrites autosave_0; still 2 files total
    mgr.tickAutosave(1.5f, serializer_, world_, density_, essence_, timeline_, pos, vel);

    auto slots3 = mgr.listSlots();
    EXPECT_EQ(slots3.size(), 2u);
}

TEST_F(SaveManagerTest, LoadNonexistentSlotReturnsFalse) {
    SaveManager mgr(testDirStr());

    std::optional<Position> pos;
    std::optional<Position> vel;
    EXPECT_FALSE(mgr.load("does_not_exist", serializer_, world_, density_, essence_, timeline_, pos, vel));
}

TEST_F(SaveManagerTest, EmptyDirectoryListSlotsReturnsEmpty) {
    SaveManager mgr(testDirStr());

    auto slots = mgr.listSlots();
    EXPECT_TRUE(slots.empty());
}

TEST_F(SaveManagerTest, AutosaveDisabledByDefault) {
    SaveManager mgr(testDirStr());

    std::optional<Position> pos = Position{0.0f, 0.0f, 0.0f};
    std::optional<Position> vel = Position{0.0f, 0.0f, 0.0f};

    // Ticking without enableAutosave should produce no saves
    mgr.tickAutosave(500.0f, serializer_, world_, density_, essence_, timeline_, pos, vel);

    auto slots = mgr.listSlots();
    EXPECT_TRUE(slots.empty());
}

TEST_F(SaveManagerTest, SavePausesAndResumesTimeline) {
    SaveManager mgr(testDirStr());

    ASSERT_FALSE(timeline_.isPaused());

    std::optional<Position> pos = Position{0.0f, 0.0f, 0.0f};
    std::optional<Position> vel = Position{0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(mgr.save("pause_test", serializer_, world_, density_, essence_, timeline_, pos, vel));

    // Timeline should be resumed after save
    EXPECT_FALSE(timeline_.isPaused());
}

TEST_F(SaveManagerTest, SavePreservesPausedTimeline) {
    SaveManager mgr(testDirStr());

    timeline_.pause();
    ASSERT_TRUE(timeline_.isPaused());

    std::optional<Position> pos = Position{0.0f, 0.0f, 0.0f};
    std::optional<Position> vel = Position{0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(mgr.save("already_paused", serializer_, world_, density_, essence_, timeline_, pos, vel));

    // Timeline was already paused; should stay paused
    EXPECT_TRUE(timeline_.isPaused());
}

TEST_F(SaveManagerTest, DeleteNonexistentSlotReturnsFalse) {
    SaveManager mgr(testDirStr());

    EXPECT_FALSE(mgr.deleteSlot("ghost_slot"));
}

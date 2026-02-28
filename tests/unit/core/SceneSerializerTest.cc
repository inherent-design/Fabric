#include "fabric/core/SceneSerializer.hh"
#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Temporal.hh"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace fabric;

class SceneSerializerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        world.registerCoreComponents();
        testFile = "/tmp/fabric_test_scene.json";
    }

    void TearDown() override { std::filesystem::remove(testFile); }

    World world;
    DensityField density;
    EssenceField essence;
    Timeline timeline;
    SceneSerializer serializer;
    std::string testFile;
};

TEST_F(SceneSerializerTest, EmptySceneSerialization) {
    nlohmann::json json = serializer.serialize(world, density, essence, timeline);

    EXPECT_TRUE(json.contains("version"));
    EXPECT_TRUE(json.contains("entities"));
    EXPECT_TRUE(json.contains("chunks"));
    EXPECT_TRUE(json.contains("timeline"));
    EXPECT_EQ(json["version"], "1.0");
    EXPECT_EQ(json["entities"].size(), 0);
}

TEST_F(SceneSerializerTest, SingleEntitySerialization) {
    auto entity = world.createSceneEntity("test_entity");
    entity.set<Position>(Position{1.0f, 2.0f, 3.0f});
    entity.set<Rotation>(Rotation{0.0f, 0.0f, 0.0f, 1.0f});
    entity.set<Scale>(Scale{1.0f, 1.0f, 1.0f});

    world.progress(0.0f);
    nlohmann::json json = serializer.serializeEntities(world);

    ASSERT_EQ(json.size(), 1);
    EXPECT_EQ(json[0]["name"], "test_entity");
    EXPECT_EQ(json[0]["components"]["Position"]["x"], 1.0);
    EXPECT_EQ(json[0]["components"]["Position"]["y"], 2.0);
    EXPECT_EQ(json[0]["components"]["Position"]["z"], 3.0);
}

TEST_F(SceneSerializerTest, MultipleEntitiesSerialization) {
    auto entity1 = world.createSceneEntity("entity1");
    entity1.set<Position>(Position{0.0f, 0.0f, 0.0f});

    auto entity2 = world.createSceneEntity("entity2");
    entity2.set<Position>(Position{10.0f, 20.0f, 30.0f});

    world.progress(0.0f);
    nlohmann::json json = serializer.serializeEntities(world);

    EXPECT_EQ(json.size(), 2);
}

TEST_F(SceneSerializerTest, EntityWithAllComponents) {
    auto entity = world.createSceneEntity("full_entity");
    entity.set<Position>(Position{1.0f, 2.0f, 3.0f});
    entity.set<Rotation>(Rotation{0.1f, 0.2f, 0.3f, 0.4f});
    entity.set<Scale>(Scale{2.0f, 2.0f, 2.0f});
    entity.set<BoundingBox>(BoundingBox{-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f});
    entity.set<LocalToWorld>(LocalToWorld{});

    world.progress(0.0f);
    nlohmann::json json = serializer.serializeEntities(world);
    ASSERT_EQ(json.size(), 1);

    const auto& components = json[0]["components"];
    EXPECT_TRUE(components.contains("Position"));
    EXPECT_TRUE(components.contains("Rotation"));
    EXPECT_TRUE(components.contains("Scale"));
    EXPECT_TRUE(components.contains("BoundingBox"));
    EXPECT_TRUE(components.contains("LocalToWorld"));
}

TEST_F(SceneSerializerTest, EntityRoundTrip) {
    auto originalEntity = world.createSceneEntity("roundtrip_test");
    originalEntity.set<Position>(Position{5.5f, 10.5f, -3.5f});
    originalEntity.set<Rotation>(Rotation{0.0f, 0.707f, 0.0f, 0.707f});
    originalEntity.set<Scale>(Scale{0.5f, 2.0f, 1.5f});

    nlohmann::json entitiesJson = serializer.serializeEntities(world);

    ASSERT_TRUE(serializer.deserializeEntities(entitiesJson, world));

    auto restoredEntity = world.get().lookup("roundtrip_test");
    ASSERT_TRUE(restoredEntity.is_valid());

    const auto* pos = restoredEntity.try_get<Position>();
    ASSERT_TRUE(pos);
    EXPECT_FLOAT_EQ(pos->x, 5.5f);
    EXPECT_FLOAT_EQ(pos->y, 10.5f);
    EXPECT_FLOAT_EQ(pos->z, -3.5f);

    const auto* rot = restoredEntity.try_get<Rotation>();
    ASSERT_TRUE(rot);
    EXPECT_FLOAT_EQ(rot->x, 0.0f);
    EXPECT_NEAR(rot->y, 0.707f, 0.001f);
    EXPECT_NEAR(rot->z, 0.0f, 0.001f);
    EXPECT_NEAR(rot->w, 0.707f, 0.001f);

    const auto* scale = restoredEntity.try_get<Scale>();
    ASSERT_TRUE(scale);
    EXPECT_FLOAT_EQ(scale->x, 0.5f);
    EXPECT_FLOAT_EQ(scale->y, 2.0f);
    EXPECT_FLOAT_EQ(scale->z, 1.5f);
}

TEST_F(SceneSerializerTest, ChunkSerialization) {
    density.write(0, 0, 0, 1.0f);
    density.write(1, 0, 0, 2.0f);
    density.write(0, 1, 0, 3.0f);

    essence.write(0, 0, 0, Vector4<float, Space::World>{1.0f, 0.0f, 0.0f, 1.0f});
    essence.write(1, 0, 0, Vector4<float, Space::World>{0.0f, 1.0f, 0.0f, 1.0f});

    nlohmann::json json = serializer.serializeChunks(density, essence);

    EXPECT_GT(json.size(), 0);
}

TEST_F(SceneSerializerTest, ChunkRoundTrip) {
    float originalDensity = 0.75f;
    Vector4<float, Space::World> originalEssence{0.5f, 0.6f, 0.7f, 0.8f};

    density.write(5, 5, 5, originalDensity);
    essence.write(5, 5, 5, originalEssence);

    nlohmann::json chunksJson = serializer.serializeChunks(density, essence);

    ASSERT_TRUE(serializer.deserializeChunks(chunksJson, density, essence));

    float restoredDensity = density.read(5, 5, 5);
    EXPECT_FLOAT_EQ(restoredDensity, originalDensity);

    auto restoredEssence = essence.read(5, 5, 5);
    EXPECT_FLOAT_EQ(restoredEssence.x, originalEssence.x);
    EXPECT_FLOAT_EQ(restoredEssence.y, originalEssence.y);
    EXPECT_FLOAT_EQ(restoredEssence.z, originalEssence.z);
    EXPECT_FLOAT_EQ(restoredEssence.w, originalEssence.w);
}

TEST_F(SceneSerializerTest, TimelineSerialization) {
    timeline.setGlobalTimeScale(2.0);
    timeline.update(1.0);

    nlohmann::json json = serializer.serializeTimeline(timeline);

    EXPECT_TRUE(json.contains("currentTime"));
    EXPECT_TRUE(json.contains("globalTimeScale"));
    EXPECT_TRUE(json.contains("isPaused"));
    EXPECT_FLOAT_EQ(json["currentTime"], 2.0);
    EXPECT_FLOAT_EQ(json["globalTimeScale"], 2.0);
    EXPECT_FALSE(json["isPaused"]);
}

TEST_F(SceneSerializerTest, TimelineRoundTrip) {
    timeline.setGlobalTimeScale(0.5);
    timeline.pause();
    timeline.update(10.0);

    nlohmann::json timelineJson = serializer.serializeTimeline(timeline);

    Timeline newTimeline;
    ASSERT_TRUE(serializer.deserializeTimeline(timelineJson, newTimeline));

    EXPECT_FLOAT_EQ(newTimeline.getCurrentTime(), 0.0);
    EXPECT_FLOAT_EQ(newTimeline.getGlobalTimeScale(), 0.5);
    EXPECT_TRUE(newTimeline.isPaused());
}

TEST_F(SceneSerializerTest, PlayerStateSerialization) {
    Position playerPos{100.0f, 200.0f, 300.0f};
    Position playerVel{1.0f, 2.0f, 3.0f};

    nlohmann::json json = serializer.serialize(world, density, essence, timeline, playerPos, playerVel);

    EXPECT_TRUE(json.contains("player"));
    EXPECT_TRUE(json["player"].contains("position"));
    EXPECT_TRUE(json["player"].contains("velocity"));
    EXPECT_EQ(json["player"]["position"]["x"], 100.0);
    EXPECT_EQ(json["player"]["position"]["y"], 200.0);
    EXPECT_EQ(json["player"]["position"]["z"], 300.0);
    EXPECT_EQ(json["player"]["velocity"]["x"], 1.0);
    EXPECT_EQ(json["player"]["velocity"]["y"], 2.0);
    EXPECT_EQ(json["player"]["velocity"]["z"], 3.0);
}

TEST_F(SceneSerializerTest, PlayerStateRoundTrip) {
    nlohmann::json json = serializer.serialize(world, density, essence, timeline);
    json["player"] = nlohmann::json{{"position", {{"x", 50.0}, {"y", 60.0}, {"z", 70.0}}},
                                    {"velocity", {{"x", -1.0}, {"y", 0.5}, {"z", 2.5}}}};

    std::optional<Position> playerPos;
    std::optional<Position> playerVel;
    ASSERT_TRUE(serializer.deserialize(json, world, density, essence, timeline, playerPos, playerVel));

    ASSERT_TRUE(playerPos);
    EXPECT_FLOAT_EQ(playerPos->x, 50.0f);
    EXPECT_FLOAT_EQ(playerPos->y, 60.0f);
    EXPECT_FLOAT_EQ(playerPos->z, 70.0f);

    ASSERT_TRUE(playerVel);
    EXPECT_FLOAT_EQ(playerVel->x, -1.0f);
    EXPECT_FLOAT_EQ(playerVel->y, 0.5f);
    EXPECT_FLOAT_EQ(playerVel->z, 2.5f);
}

TEST_F(SceneSerializerTest, FullSceneRoundTrip) {
    auto entity = world.createSceneEntity("test");
    entity.set<Position>(Position{1.0f, 2.0f, 3.0f});
    density.write(0, 0, 0, 0.5f);
    timeline.setGlobalTimeScale(1.5);

    world.progress(0.0f);
    nlohmann::json json = serializer.serialize(world, density, essence, timeline);

    World newWorld;
    newWorld.registerCoreComponents();
    DensityField newDensity;
    EssenceField newEssence;
    Timeline newTimeline;
    std::optional<Position> newPlayerPos;
    std::optional<Position> newPlayerVel;

    ASSERT_TRUE(
        serializer.deserialize(json, newWorld, newDensity, newEssence, newTimeline, newPlayerPos, newPlayerVel));

    EXPECT_FLOAT_EQ(newDensity.read(0, 0, 0), 0.5f);
    EXPECT_FLOAT_EQ(newTimeline.getGlobalTimeScale(), 1.5f);
}

TEST_F(SceneSerializerTest, SaveToFile) {
    nlohmann::json json = serializer.serialize(world, density, essence, timeline);

    ASSERT_TRUE(serializer.saveToFile(testFile, json));

    std::ifstream file(testFile);
    ASSERT_TRUE(file.is_open());

    nlohmann::json loadedJson;
    file >> loadedJson;
    file.close();

    EXPECT_EQ(json.dump(), loadedJson.dump());
}

TEST_F(SceneSerializerTest, LoadFromFile) {
    nlohmann::json originalJson;
    originalJson["version"] = "1.0";
    originalJson["testKey"] = "testValue";

    ASSERT_TRUE(serializer.saveToFile(testFile, originalJson));

    auto loadedJson = serializer.loadFromFile(testFile);
    ASSERT_TRUE(loadedJson);

    EXPECT_EQ((*loadedJson)["version"], "1.0");
    EXPECT_EQ((*loadedJson)["testKey"], "testValue");
}

TEST_F(SceneSerializerTest, LoadNonexistentFile) {
    auto loadedJson = serializer.loadFromFile("/tmp/nonexistent_file_12345.json");
    EXPECT_FALSE(loadedJson);
}

TEST_F(SceneSerializerTest, DeserializeInvalidJson) {
    nlohmann::json invalidJson;
    std::optional<Position> playerPos;
    std::optional<Position> playerVel;

    EXPECT_FALSE(serializer.deserialize(invalidJson, world, density, essence, timeline, playerPos, playerVel));
}

TEST_F(SceneSerializerTest, DeserializeMissingVersion) {
    nlohmann::json json;
    json["entities"] = nlohmann::json::array();
    std::optional<Position> playerPos;
    std::optional<Position> playerVel;

    EXPECT_FALSE(serializer.deserialize(json, world, density, essence, timeline, playerPos, playerVel));
}

TEST_F(SceneSerializerTest, SceneConfigHelpers) {
    nlohmann::json json;
    json["version"] = "1.0";
    json["entities"] = nlohmann::json::array();
    json["chunks"] = nlohmann::json::array();
    json["timeline"] = nlohmann::json{};

    SceneConfig config = SceneConfig::fromJson(json);

    EXPECT_EQ(config.entities.size(), 0);
    EXPECT_EQ(config.chunks.size(), 0);
    EXPECT_FALSE(config.player);

    nlohmann::json outputJson = config.toJson();
    EXPECT_EQ(outputJson.dump(), json.dump());
}

TEST_F(SceneSerializerTest, PartialSerializationEntitiesOnly) {
    auto entity = world.createSceneEntity("partial_test");
    entity.set<Position>(Position{7.0f, 8.0f, 9.0f});

    nlohmann::json entitiesJson = serializer.serializeEntities(world);

    ASSERT_TRUE(serializer.deserializeEntities(entitiesJson, world));

    auto restored = world.get().lookup("partial_test");
    ASSERT_TRUE(restored.is_valid());

    const auto* pos = restored.try_get<Position>();
    ASSERT_TRUE(pos);
    EXPECT_FLOAT_EQ(pos->x, 7.0f);
    EXPECT_FLOAT_EQ(pos->y, 8.0f);
    EXPECT_FLOAT_EQ(pos->z, 9.0f);
}

TEST_F(SceneSerializerTest, ParentChildRelationship) {
    auto parent = world.createSceneEntity("parent");
    parent.set<Position>(Position{0.0f, 0.0f, 0.0f});

    auto child = world.createChildEntity(parent, "child");
    child.set<Position>(Position{1.0f, 0.0f, 0.0f});

    world.progress(0.0f);
    nlohmann::json entitiesJson = serializer.serializeEntities(world);
    ASSERT_EQ(entitiesJson.size(), 2);

    ASSERT_TRUE(serializer.deserializeEntities(entitiesJson, world));

    auto restoredParent = world.get().lookup("parent");
    ASSERT_TRUE(restoredParent.is_valid());

    auto restoredChild = world.get().lookup("parent.child");
    if (!restoredChild.is_valid()) {
        restoredChild = world.get().lookup("parent::child");
    }
    if (!restoredChild.is_valid()) {
        restoredChild = world.get().lookup("child");
    }

    ASSERT_TRUE(restoredChild.is_valid());
    EXPECT_TRUE(restoredChild.has(flecs::ChildOf, restoredParent));
    EXPECT_EQ(restoredChild.parent(), restoredParent);
}

TEST_F(SceneSerializerTest, RenderableComponent) {
    auto entity = world.createSceneEntity("renderable");
    entity.set<Renderable>(Renderable{42});

    world.progress(0.0f);
    nlohmann::json entitiesJson = serializer.serializeEntities(world);
    ASSERT_EQ(entitiesJson.size(), 1);
    EXPECT_EQ(entitiesJson[0]["components"]["Renderable"], 42);

    ASSERT_TRUE(serializer.deserializeEntities(entitiesJson, world));

    auto restored = world.get().lookup("renderable");
    const auto* renderable = restored.try_get<Renderable>();
    ASSERT_TRUE(renderable);
    EXPECT_EQ(renderable->sortKey, 42);
}

TEST_F(SceneSerializerTest, EmptyEntitiesDeserialize) {
    nlohmann::json entitiesJson = nlohmann::json::array();

    EXPECT_TRUE(serializer.deserializeEntities(entitiesJson, world));
}

TEST_F(SceneSerializerTest, EmptyChunksDeserialize) {
    nlohmann::json chunksJson = nlohmann::json::array();

    EXPECT_TRUE(serializer.deserializeChunks(chunksJson, density, essence));
}

TEST_F(SceneSerializerTest, TimelinePausedSerialization) {
    timeline.setGlobalTimeScale(1.0);
    timeline.pause();

    nlohmann::json timelineJson = serializer.serializeTimeline(timeline);

    Timeline newTimeline;
    ASSERT_TRUE(serializer.deserializeTimeline(timelineJson, newTimeline));

    EXPECT_TRUE(newTimeline.isPaused());
}

TEST_F(SceneSerializerTest, PhysicsBodyRoundTrip) {
    auto entity = world.createSceneEntity("physics_entity");
    entity.set<PhysicsBodyConfig>(PhysicsBodyConfig{PhysicsShapeType::Sphere, 5.0f, 0.8f, 0.2f, 1.0f, -2.0f, 3.0f});

    world.progress(0.0f);
    nlohmann::json entitiesJson = serializer.serializeEntities(world);
    ASSERT_EQ(entitiesJson.size(), 1);

    const auto& physJson = entitiesJson[0]["components"]["PhysicsBody"];
    EXPECT_EQ(physJson["shapeType"], "sphere");
    EXPECT_FLOAT_EQ(physJson["mass"], 5.0f);
    EXPECT_FLOAT_EQ(physJson["restitution"], 0.8f);
    EXPECT_FLOAT_EQ(physJson["friction"], 0.2f);
    EXPECT_FLOAT_EQ(physJson["velocity"]["x"], 1.0f);
    EXPECT_FLOAT_EQ(physJson["velocity"]["y"], -2.0f);
    EXPECT_FLOAT_EQ(physJson["velocity"]["z"], 3.0f);

    ASSERT_TRUE(serializer.deserializeEntities(entitiesJson, world));

    auto restored = world.get().lookup("physics_entity");
    ASSERT_TRUE(restored.is_valid());

    const auto* phys = restored.try_get<PhysicsBodyConfig>();
    ASSERT_TRUE(phys);
    EXPECT_EQ(phys->shapeType, PhysicsShapeType::Sphere);
    EXPECT_FLOAT_EQ(phys->mass, 5.0f);
    EXPECT_FLOAT_EQ(phys->restitution, 0.8f);
    EXPECT_FLOAT_EQ(phys->friction, 0.2f);
    EXPECT_FLOAT_EQ(phys->velocityX, 1.0f);
    EXPECT_FLOAT_EQ(phys->velocityY, -2.0f);
    EXPECT_FLOAT_EQ(phys->velocityZ, 3.0f);
}

TEST_F(SceneSerializerTest, AIBehaviorRoundTrip) {
    auto entity = world.createSceneEntity("ai_entity");
    AIBehaviorConfig aiConfig;
    aiConfig.btXmlId = "patrol_tree";
    aiConfig.currentState = 2; // Chase
    aiConfig.waypoints = {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 5.0f}, {20.0f, 0.0f, -3.0f}};
    entity.set<AIBehaviorConfig>(aiConfig);

    world.progress(0.0f);
    nlohmann::json entitiesJson = serializer.serializeEntities(world);
    ASSERT_EQ(entitiesJson.size(), 1);

    const auto& aiJson = entitiesJson[0]["components"]["AIBehavior"];
    EXPECT_EQ(aiJson["btXmlId"], "patrol_tree");
    EXPECT_EQ(aiJson["currentState"], 2);
    ASSERT_EQ(aiJson["waypoints"].size(), 3);
    EXPECT_FLOAT_EQ(aiJson["waypoints"][1]["x"], 10.0f);

    ASSERT_TRUE(serializer.deserializeEntities(entitiesJson, world));

    auto restored = world.get().lookup("ai_entity");
    ASSERT_TRUE(restored.is_valid());

    const auto* ai = restored.try_get<AIBehaviorConfig>();
    ASSERT_TRUE(ai);
    EXPECT_EQ(ai->btXmlId, "patrol_tree");
    EXPECT_EQ(ai->currentState, 2);
    ASSERT_EQ(ai->waypoints.size(), 3);
    EXPECT_FLOAT_EQ(ai->waypoints[0][0], 0.0f);
    EXPECT_FLOAT_EQ(ai->waypoints[1][0], 10.0f);
    EXPECT_FLOAT_EQ(ai->waypoints[1][2], 5.0f);
    EXPECT_FLOAT_EQ(ai->waypoints[2][0], 20.0f);
    EXPECT_FLOAT_EQ(ai->waypoints[2][2], -3.0f);
}

TEST_F(SceneSerializerTest, AudioSourceRoundTrip) {
    auto entity = world.createSceneEntity("audio_entity");
    entity.set<AudioSourceConfig>(AudioSourceConfig{"sounds/ambient.wav", 0.75f, true, 5.0f, 10.0f, -2.0f});

    world.progress(0.0f);
    nlohmann::json entitiesJson = serializer.serializeEntities(world);
    ASSERT_EQ(entitiesJson.size(), 1);

    const auto& audioJson = entitiesJson[0]["components"]["AudioSource"];
    EXPECT_EQ(audioJson["soundPath"], "sounds/ambient.wav");
    EXPECT_FLOAT_EQ(audioJson["volume"], 0.75f);
    EXPECT_TRUE(audioJson["looping"]);
    EXPECT_FLOAT_EQ(audioJson["position"]["x"], 5.0f);
    EXPECT_FLOAT_EQ(audioJson["position"]["y"], 10.0f);
    EXPECT_FLOAT_EQ(audioJson["position"]["z"], -2.0f);

    ASSERT_TRUE(serializer.deserializeEntities(entitiesJson, world));

    auto restored = world.get().lookup("audio_entity");
    ASSERT_TRUE(restored.is_valid());

    const auto* audio = restored.try_get<AudioSourceConfig>();
    ASSERT_TRUE(audio);
    EXPECT_EQ(audio->soundPath, "sounds/ambient.wav");
    EXPECT_FLOAT_EQ(audio->volume, 0.75f);
    EXPECT_TRUE(audio->looping);
    EXPECT_FLOAT_EQ(audio->positionX, 5.0f);
    EXPECT_FLOAT_EQ(audio->positionY, 10.0f);
    EXPECT_FLOAT_EQ(audio->positionZ, -2.0f);
}

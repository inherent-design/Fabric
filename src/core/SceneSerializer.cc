#include "fabric/core/SceneSerializer.hh"
#include "fabric/core/Log.hh"
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace fabric {

nlohmann::json SceneSerializer::serialize(World& world, DensityField& density, EssenceField& essence,
                                          const Timeline& timeline, const std::optional<Position>& playerPos,
                                          const std::optional<Position>& playerVel) {
    SceneConfig config;
    config.entities = serializeEntities(world);
    config.chunks = serializeChunks(density, essence);
    config.timeline = serializeTimeline(timeline);

    if (playerPos || playerVel) {
        nlohmann::json playerJson;
        if (playerPos) {
            playerJson["position"] = nlohmann::json{{"x", playerPos->x}, {"y", playerPos->y}, {"z", playerPos->z}};
        }
        if (playerVel) {
            playerJson["velocity"] = nlohmann::json{{"x", playerVel->x}, {"y", playerVel->y}, {"z", playerVel->z}};
        }
        config.player = playerJson;
    }

    return config.toJson();
}

nlohmann::json SceneSerializer::serializeEntities(World& world) {
    std::vector<nlohmann::json> entitiesJson;

    world.get().each([&](flecs::entity e, const SceneEntity&) {
        if (!e.is_valid() || !e.is_alive()) {
            return;
        }
        nlohmann::json entityJson = serializeEntity(e);
        if (!entityJson.empty()) {
            entitiesJson.push_back(std::move(entityJson));
        }
    });

    return entitiesJson;
}

nlohmann::json SceneSerializer::serializeChunks(DensityField& density, EssenceField& essence) {
    std::vector<nlohmann::json> chunksJson;

    auto activeChunks = density.grid().activeChunks();
    for (const auto& [cx, cy, cz] : activeChunks) {
        nlohmann::json chunkJson;
        chunkJson["x"] = cx;
        chunkJson["y"] = cy;
        chunkJson["z"] = cz;

        std::vector<float> densityData;
        std::vector<float> essenceData;

        const_cast<ChunkedGrid<float>&>(density.grid()).forEachCell(cx, cy, cz, [&](int wx, int wy, int wz, float d) {
            densityData.push_back(d);
        });

        const_cast<ChunkedGrid<Vector4<float, Space::World>>&>(essence.grid())
            .forEachCell(cx, cy, cz, [&](int wx, int wy, int wz, const Vector4<float, Space::World>& e) {
                essenceData.push_back(e.x);
                essenceData.push_back(e.y);
                essenceData.push_back(e.z);
                essenceData.push_back(e.w);
            });

        chunkJson["density"] = densityData;
        chunkJson["essence"] = essenceData;
        chunksJson.push_back(std::move(chunkJson));
    }

    return chunksJson;
}

nlohmann::json SceneSerializer::serializeTimeline(const Timeline& timeline) {
    nlohmann::json timelineJson;

    timelineJson["currentTime"] = timeline.getCurrentTime();
    timelineJson["globalTimeScale"] = timeline.getGlobalTimeScale();
    timelineJson["isPaused"] = timeline.isPaused();

    const auto& history = timeline.getHistory();
    std::vector<nlohmann::json> historyJson;
    for (const auto& state : history) {
        historyJson.push_back(serializeTimeState(state));
    }
    timelineJson["history"] = historyJson;

    return timelineJson;
}

nlohmann::json SceneSerializer::serializeEntity(flecs::entity entity) {
    nlohmann::json entityJson;

    entityJson["id"] = static_cast<uint64_t>(entity.id());

    const char* name = entity.name();
    if (name) {
        entityJson["name"] = std::string(name);
    }

    auto parent = entity.parent();
    if (parent.is_valid()) {
        entityJson["parentId"] = static_cast<uint64_t>(parent.id());
    }

    nlohmann::json componentsJson = serializeComponents(entity);
    if (!componentsJson.empty()) {
        entityJson["components"] = componentsJson;
    }

    return entityJson;
}

nlohmann::json SceneSerializer::serializeComponents(flecs::entity entity) {
    nlohmann::json componentsJson;

    auto pos = entity.get_ref<Position>();
    if (pos) {
        componentsJson["Position"] = nlohmann::json{{"x", pos->x}, {"y", pos->y}, {"z", pos->z}};
    }

    auto rot = entity.get_ref<Rotation>();
    if (rot) {
        componentsJson["Rotation"] = nlohmann::json{{"x", rot->x}, {"y", rot->y}, {"z", rot->z}, {"w", rot->w}};
    }

    auto scale = entity.get_ref<Scale>();
    if (scale) {
        componentsJson["Scale"] = nlohmann::json{{"x", scale->x}, {"y", scale->y}, {"z", scale->z}};
    }

    auto bbox = entity.get_ref<BoundingBox>();
    if (bbox) {
        componentsJson["BoundingBox"] =
            nlohmann::json{{"minX", bbox->minX}, {"minY", bbox->minY}, {"minZ", bbox->minZ},
                           {"maxX", bbox->maxX}, {"maxY", bbox->maxY}, {"maxZ", bbox->maxZ}};
    }

    auto ltw = entity.get_ref<LocalToWorld>();
    if (ltw) {
        std::vector<float> matrix;
        for (float f : ltw->matrix) {
            matrix.push_back(f);
        }
        componentsJson["LocalToWorld"] = matrix;
    }

    if (entity.has<SceneEntity>()) {
        componentsJson["SceneEntity"] = true;
    }

    auto renderable = entity.get_ref<Renderable>();
    if (renderable) {
        componentsJson["Renderable"] = renderable->sortKey;
    }

    auto physBody = entity.get_ref<PhysicsBodyConfig>();
    if (physBody) {
        nlohmann::json physJson;
        switch (physBody->shapeType) {
            case PhysicsShapeType::Sphere:
                physJson["shapeType"] = "sphere";
                break;
            case PhysicsShapeType::Capsule:
                physJson["shapeType"] = "capsule";
                break;
            case PhysicsShapeType::Mesh:
                physJson["shapeType"] = "mesh";
                break;
            default:
                physJson["shapeType"] = "box";
                break;
        }
        physJson["mass"] = physBody->mass;
        physJson["restitution"] = physBody->restitution;
        physJson["friction"] = physBody->friction;
        physJson["velocity"] =
            nlohmann::json{{"x", physBody->velocityX}, {"y", physBody->velocityY}, {"z", physBody->velocityZ}};
        componentsJson["PhysicsBody"] = physJson;
    }

    auto aiBehavior = entity.get_ref<AIBehaviorConfig>();
    if (aiBehavior) {
        nlohmann::json aiJson;
        aiJson["btXmlId"] = aiBehavior->btXmlId;
        aiJson["currentState"] = aiBehavior->currentState;
        nlohmann::json waypointsJson = nlohmann::json::array();
        for (const auto& wp : aiBehavior->waypoints) {
            waypointsJson.push_back(nlohmann::json{{"x", wp[0]}, {"y", wp[1]}, {"z", wp[2]}});
        }
        aiJson["waypoints"] = waypointsJson;
        componentsJson["AIBehavior"] = aiJson;
    }

    auto audioSource = entity.get_ref<AudioSourceConfig>();
    if (audioSource) {
        nlohmann::json audioJson;
        audioJson["soundPath"] = audioSource->soundPath;
        audioJson["volume"] = audioSource->volume;
        audioJson["looping"] = audioSource->looping;
        audioJson["position"] =
            nlohmann::json{{"x", audioSource->positionX}, {"y", audioSource->positionY}, {"z", audioSource->positionZ}};
        componentsJson["AudioSource"] = audioJson;
    }

    return componentsJson;
}

nlohmann::json SceneSerializer::serializeTimeState(const TimeState& state) {
    nlohmann::json stateJson;
    stateJson["timestamp"] = state.getTimestamp();
    return stateJson;
}

bool SceneSerializer::deserialize(const nlohmann::json& json, World& world, DensityField& density,
                                  EssenceField& essence, Timeline& timeline, std::optional<Position>& playerPos,
                                  std::optional<Position>& playerVel) {
    if (!json.contains("version")) {
        FABRIC_LOG_ERROR("Invalid scene JSON: missing version field");
        return false;
    }

    if (json.contains("entities")) {
        if (!deserializeEntities(json["entities"], world)) {
            return false;
        }
    }

    if (json.contains("chunks")) {
        if (!deserializeChunks(json["chunks"], density, essence)) {
            return false;
        }
    }

    if (json.contains("timeline")) {
        if (!deserializeTimeline(json["timeline"], timeline)) {
            return false;
        }
    }

    if (json.contains("player")) {
        const auto& playerJson = json["player"];
        if (playerJson.contains("position")) {
            const auto& posJson = playerJson["position"];
            playerPos = Position{posJson.value("x", 0.0f), posJson.value("y", 0.0f), posJson.value("z", 0.0f)};
        }
        if (playerJson.contains("velocity")) {
            const auto& velJson = playerJson["velocity"];
            playerVel = Position{velJson.value("x", 0.0f), velJson.value("y", 0.0f), velJson.value("z", 0.0f)};
        }
    }

    FABRIC_LOG_INFO("Scene deserialized successfully");
    return true;
}

bool SceneSerializer::deserializeEntities(const nlohmann::json& json, World& world) {
    if (!json.is_array()) {
        FABRIC_LOG_ERROR("Entities JSON is not an array");
        return false;
    }

    std::unordered_map<uint64_t, flecs::entity> idMap;
    idMap.reserve(json.size());

    for (const auto& entityJson : json) {
        if (!entityJson.contains("id")) {
            FABRIC_LOG_WARN("Skipping entity without ID");
            continue;
        }

        const uint64_t oldId = entityJson["id"];
        flecs::entity entity = createEntity(world, entityJson);
        if (!entity.is_valid()) {
            FABRIC_LOG_WARN("Failed to create entity from JSON");
            continue;
        }

        idMap.emplace(oldId, entity);
    }

    for (const auto& entityJson : json) {
        if (!entityJson.contains("id") || !entityJson.contains("parentId")) {
            continue;
        }

        const uint64_t childOldId = entityJson["id"];
        const uint64_t parentOldId = entityJson["parentId"];
        const auto childIt = idMap.find(childOldId);
        const auto parentIt = idMap.find(parentOldId);
        if (childIt != idMap.end() && parentIt != idMap.end()) {
            childIt->second.child_of(parentIt->second);
        }
    }

    return true;
}

bool SceneSerializer::deserializeChunks(const nlohmann::json& json, DensityField& density, EssenceField& essence) {
    if (!json.is_array()) {
        FABRIC_LOG_ERROR("Chunks JSON is not an array");
        return false;
    }

    for (const auto& chunkJson : json) {
        if (!chunkJson.contains("x") || !chunkJson.contains("y") || !chunkJson.contains("z")) {
            FABRIC_LOG_WARN("Skipping chunk without coordinates");
            continue;
        }

        int cx = chunkJson["x"];
        int cy = chunkJson["y"];
        int cz = chunkJson["z"];

        if (chunkJson.contains("density")) {
            const auto& densityArray = chunkJson["density"];
            if (densityArray.is_array()) {
                const size_t maxCells =
                    static_cast<size_t>(kChunkSize) * static_cast<size_t>(kChunkSize) * static_cast<size_t>(kChunkSize);
                const size_t cellCount = std::min(densityArray.size(), maxCells);
                const int baseX = cx * kChunkSize;
                const int baseY = cy * kChunkSize;
                const int baseZ = cz * kChunkSize;

                for (size_t i = 0; i < cellCount; ++i) {
                    const int lx = static_cast<int>(i % kChunkSize);
                    const int ly = static_cast<int>((i / kChunkSize) % kChunkSize);
                    const int lz = static_cast<int>(i / (kChunkSize * kChunkSize));
                    density.write(baseX + lx, baseY + ly, baseZ + lz, densityArray[i]);
                }
            }
        }

        if (chunkJson.contains("essence")) {
            const auto& essenceArray = chunkJson["essence"];
            if (essenceArray.is_array() && essenceArray.size() % 4 == 0) {
                const size_t maxCells =
                    static_cast<size_t>(kChunkSize) * static_cast<size_t>(kChunkSize) * static_cast<size_t>(kChunkSize);
                const size_t vecCount = std::min(essenceArray.size() / 4, maxCells);
                const int baseX = cx * kChunkSize;
                const int baseY = cy * kChunkSize;
                const int baseZ = cz * kChunkSize;

                for (size_t i = 0; i < vecCount; ++i) {
                    const size_t index = i * 4;
                    const int lx = static_cast<int>(i % kChunkSize);
                    const int ly = static_cast<int>((i / kChunkSize) % kChunkSize);
                    const int lz = static_cast<int>(i / (kChunkSize * kChunkSize));

                    Vector4<float, Space::World> e{essenceArray[index], essenceArray[index + 1],
                                                   essenceArray[index + 2], essenceArray[index + 3]};
                    essence.write(baseX + lx, baseY + ly, baseZ + lz, e);
                }
            }
        }
    }

    return true;
}

bool SceneSerializer::deserializeTimeline(const nlohmann::json& json, Timeline& timeline) {
    if (!json.contains("currentTime")) {
        FABRIC_LOG_ERROR("Timeline JSON missing currentTime");
        return false;
    }

    timeline.pause();

    if (json.contains("globalTimeScale")) {
        timeline.setGlobalTimeScale(json["globalTimeScale"]);
    }

    if (json.contains("isPaused") && !json["isPaused"]) {
        timeline.resume();
    }

    return true;
}

flecs::entity SceneSerializer::createEntity(World& world, const nlohmann::json& entityJson) {
    flecs::entity entity;

    if (entityJson.contains("id")) {
        uint64_t entityId = entityJson["id"];
        flecs::entity byId = world.get().entity(entityId);
        if (byId.is_valid() && byId.is_alive()) {
            entity = byId;
        }
    }

    if (!entity.is_valid() && entityJson.contains("name")) {
        std::string name = entityJson["name"];
        flecs::entity byName = world.get().lookup(name.c_str());
        if (byName.is_valid() && byName.is_alive()) {
            entity = byName;
        }
    }

    if (!entity.is_valid()) {
        entity = world.get().entity();
    }

    if (entityJson.contains("name") && !entity.name()) {
        std::string name = entityJson["name"];
        entity.set_name(name.c_str());
    }

    if (entityJson.contains("components")) {
        restoreComponents(entity, entityJson["components"]);
    }

    return entity;
}

bool SceneSerializer::restoreComponents(flecs::entity entity, const nlohmann::json& componentsJson) {
    if (!componentsJson.is_object()) {
        return false;
    }

    if (componentsJson.contains("Position")) {
        const auto& posJson = componentsJson["Position"];
        entity.set<Position>(Position{posJson.value("x", 0.0f), posJson.value("y", 0.0f), posJson.value("z", 0.0f)});
    }

    if (componentsJson.contains("Rotation")) {
        const auto& rotJson = componentsJson["Rotation"];
        entity.set<Rotation>(Rotation{rotJson.value("x", 0.0f), rotJson.value("y", 0.0f), rotJson.value("z", 0.0f),
                                      rotJson.value("w", 1.0f)});
    }

    if (componentsJson.contains("Scale")) {
        const auto& scaleJson = componentsJson["Scale"];
        entity.set<Scale>(Scale{scaleJson.value("x", 1.0f), scaleJson.value("y", 1.0f), scaleJson.value("z", 1.0f)});
    }

    if (componentsJson.contains("BoundingBox")) {
        const auto& bboxJson = componentsJson["BoundingBox"];
        entity.set<BoundingBox>(BoundingBox{bboxJson.value("minX", 0.0f), bboxJson.value("minY", 0.0f),
                                            bboxJson.value("minZ", 0.0f), bboxJson.value("maxX", 0.0f),
                                            bboxJson.value("maxY", 0.0f), bboxJson.value("maxZ", 0.0f)});
    }

    if (componentsJson.contains("LocalToWorld")) {
        const auto& matrixJson = componentsJson["LocalToWorld"];
        if (matrixJson.is_array() && matrixJson.size() == 16) {
            LocalToWorld ltw;
            for (size_t i = 0; i < 16; ++i) {
                ltw.matrix[i] = matrixJson[i];
            }
            entity.set<LocalToWorld>(ltw);
        }
    }

    if (componentsJson.contains("SceneEntity") && componentsJson["SceneEntity"].is_boolean()) {
        if (componentsJson["SceneEntity"]) {
            entity.add<SceneEntity>();
        }
    }

    if (componentsJson.contains("Renderable")) {
        entity.set<Renderable>(Renderable{componentsJson["Renderable"]});
    }

    if (componentsJson.contains("PhysicsBody")) {
        const auto& physJson = componentsJson["PhysicsBody"];
        PhysicsBodyConfig config;
        std::string shapeStr = physJson.value("shapeType", std::string("box"));
        if (shapeStr == "sphere") {
            config.shapeType = PhysicsShapeType::Sphere;
        } else if (shapeStr == "capsule") {
            config.shapeType = PhysicsShapeType::Capsule;
        } else if (shapeStr == "mesh") {
            config.shapeType = PhysicsShapeType::Mesh;
        } else {
            config.shapeType = PhysicsShapeType::Box;
        }
        config.mass = physJson.value("mass", 1.0f);
        config.restitution = physJson.value("restitution", 0.3f);
        config.friction = physJson.value("friction", 0.5f);
        if (physJson.contains("velocity")) {
            const auto& velJson = physJson["velocity"];
            config.velocityX = velJson.value("x", 0.0f);
            config.velocityY = velJson.value("y", 0.0f);
            config.velocityZ = velJson.value("z", 0.0f);
        }
        entity.set<PhysicsBodyConfig>(config);
    }

    if (componentsJson.contains("AIBehavior")) {
        const auto& aiJson = componentsJson["AIBehavior"];
        AIBehaviorConfig config;
        config.btXmlId = aiJson.value("btXmlId", std::string(""));
        config.currentState = aiJson.value("currentState", static_cast<uint8_t>(0));
        if (aiJson.contains("waypoints") && aiJson["waypoints"].is_array()) {
            for (const auto& wpJson : aiJson["waypoints"]) {
                std::array<float, 3> wp = {wpJson.value("x", 0.0f), wpJson.value("y", 0.0f), wpJson.value("z", 0.0f)};
                config.waypoints.push_back(wp);
            }
        }
        entity.set<AIBehaviorConfig>(config);
    }

    if (componentsJson.contains("AudioSource")) {
        const auto& audioJson = componentsJson["AudioSource"];
        AudioSourceConfig config;
        config.soundPath = audioJson.value("soundPath", std::string(""));
        config.volume = audioJson.value("volume", 1.0f);
        config.looping = audioJson.value("looping", false);
        if (audioJson.contains("position")) {
            const auto& posJson = audioJson["position"];
            config.positionX = posJson.value("x", 0.0f);
            config.positionY = posJson.value("y", 0.0f);
            config.positionZ = posJson.value("z", 0.0f);
        }
        entity.set<AudioSourceConfig>(config);
    }

    return true;
}

TimeState SceneSerializer::deserializeTimeState(const nlohmann::json& json) {
    double timestamp = json.value("timestamp", 0.0);
    return TimeState(timestamp);
}

bool SceneSerializer::saveToFile(const std::string& filepath, const nlohmann::json& json) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            FABRIC_LOG_ERROR("Failed to open file for writing: {}", filepath);
            return false;
        }

        file << json.dump(2);
        file.close();
        FABRIC_LOG_INFO("Scene saved to {}", filepath);
        return true;
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Exception saving scene to {}: {}", filepath, e.what());
        return false;
    }
}

std::optional<nlohmann::json> SceneSerializer::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            FABRIC_LOG_ERROR("Failed to open file for reading: {}", filepath);
            return std::nullopt;
        }

        nlohmann::json json;
        file >> json;
        file.close();
        FABRIC_LOG_INFO("Scene loaded from {}", filepath);
        return json;
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Exception loading scene from {}: {}", filepath, e.what());
        return std::nullopt;
    }
}

} // namespace fabric

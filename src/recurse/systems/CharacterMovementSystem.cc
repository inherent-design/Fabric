#include "recurse/systems/CharacterMovementSystem.hh"

#include "recurse/config/RecurseConfig.hh"
#include "recurse/input/ActionIds.hh"

#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/ecs/ECS.hh"
#include "fabric/ecs/WorldScoped.hh"
#include "fabric/input/InputManager.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/character/CameraController.hh"
#include "recurse/character/FlightController.hh"
#include "recurse/components/StreamSource.hh"
#include "recurse/physics/JoltCharacterController.hh"

#include <cmath>

namespace recurse::systems {

CharacterMovementSystem::~CharacterMovementSystem() = default;

void CharacterMovementSystem::doShutdown() {
    if (playerEntity_.is_valid())
        playerEntity_.destruct();

    flightCtrl_.reset();
    joltCharCtrl_ = nullptr;
    ecsWorld_ = nullptr;
    terrain_ = nullptr;
    camera_ = nullptr;
    physics_ = nullptr;
    voxelSim_ = nullptr;
    FABRIC_LOG_INFO("CharacterMovementSystem shut down");
}

void CharacterMovementSystem::setPlayerPosition(const fabric::Vec3f& pos) {
    playerPos_ = pos;
    playerPosD_ = fabric::Vector3<double, fabric::Space::World>(static_cast<double>(pos.x), static_cast<double>(pos.y),
                                                                static_cast<double>(pos.z));
    if (joltCharCtrl_) {
        joltCharCtrl_->setPosition(pos);
    }
    playerVel_ = {};

    // Sync ECS component immediately so StreamSource queries in CPS
    // read the correct position on the first frame after world load.
    if (playerEntity_.is_valid())
        playerEntity_.set<fabric::Position>({pos.x, pos.y, pos.z});
}

namespace {

constexpr float K_CHAR_WIDTH = 0.6f;
constexpr float K_CHAR_HEIGHT = 1.8f;
constexpr float K_CHAR_DEPTH = 0.6f;

void syncPlayerPositionViews(fabric::Vector3<double, fabric::Space::World>& playerPosD, fabric::Vec3f& playerPos,
                             const fabric::Vec3f& resolved) {
    playerPos = resolved;
    playerPosD = fabric::Vector3<double, fabric::Space::World>(
        static_cast<double>(resolved.x), static_cast<double>(resolved.y), static_cast<double>(resolved.z));
}

} // namespace

void CharacterMovementSystem::doInit(fabric::AppContext& ctx) {
    if (auto* wl = ctx.worldLifecycle) {
        wl->registerParticipant([this]() { onWorldBegin(); }, [this]() { onWorldEnd(); });
    }
    playerPosD_ = fabric::Vector3<double, fabric::Space::World>(
        static_cast<double>(playerPos_.x), static_cast<double>(playerPos_.y), static_cast<double>(playerPos_.z));

    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();
    physics_ = ctx.systemRegistry.get<PhysicsGameSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();

    flightCtrl_ = std::make_unique<FlightController>(K_CHAR_WIDTH, K_CHAR_HEIGHT, K_CHAR_DEPTH);

    // Jolt-based controller required for ground movement
    if (!physics_ || !physics_->physicsWorld().initialized()) {
        FABRIC_LOG_ERROR("CharacterMovementSystem: PhysicsWorld not initialized - character collision disabled");
        return;
    }

    JoltCharacterConfig config;
    config.width = K_CHAR_WIDTH;
    config.height = K_CHAR_HEIGHT;
    config.mass = 70.0f;
    config.maxSlopeAngle = charConfig_.slopeLimit;

    joltCharCtrl_ = physics_->physicsWorld().createCharacter(config);
    if (!joltCharCtrl_) {
        FABRIC_LOG_ERROR("CharacterMovementSystem: Failed to create JoltCharacterController");
        return;
    }

    joltCharCtrl_->setPosition(playerPos_);
    FABRIC_LOG_INFO("JoltCharacterController enabled for player movement");

    // Toggle fly/noclip mode: Grounded -> Flying -> Noclip -> Grounded
    using namespace recurse::input;
    ctx.dispatcher.addEventListener(K_ACTION_TOGGLE_FLY, [this](fabric::Event&) {
        if (movementFSM_.isNoclip()) {
            // Noclip -> Falling (exit to normal mode)
            movementFSM_.tryTransition(CharacterState::Falling);
            FABRIC_LOG_INFO("Noclip mode: off (falling)");
        } else if (movementFSM_.isFlying()) {
            // Flying -> Noclip
            movementFSM_.tryTransition(CharacterState::Noclip);
            FABRIC_LOG_INFO("Noclip mode: on (fly + no collision)");
        } else {
            // Grounded/Falling/Jumping -> Flying
            movementFSM_.tryTransition(CharacterState::Flying);
            playerVel_ = {};
            FABRIC_LOG_INFO("Flight mode: on");
        }
    });

    // Jump on space press (grounded only)
    ctx.dispatcher.addEventListener(K_ACTION_MOVE_UP, [this](fabric::Event&) {
        if (movementFSM_.isGrounded()) {
            if (movementFSM_.tryTransition(CharacterState::Jumping)) {
                FABRIC_LOG_DEBUG("Movement state: Grounded -> Jumping (jump input)");
                playerVel_.y = charConfig_.jumpForce;
            }
        }
    });

    ecsWorld_ = &ctx.world.get();
    streamRadius_ = ctx.configManager.get<int>("terrain.chunk_radius", 8);
    collisionRadius_ =
        ctx.configManager.get<int>("physics.collision_radius", recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS);
    playerEntity_ = ecsWorld_->entity("player")
                        .add<fabric::WorldScoped>()
                        .set<fabric::Position>({playerPos_.x, playerPos_.y, playerPos_.z})
                        .set<recurse::StreamSource>({streamRadius_, collisionRadius_});

    FABRIC_LOG_INFO("CharacterMovementSystem initialized (player entity created, stream={}, collision={})",
                    streamRadius_, collisionRadius_);
}

void CharacterMovementSystem::fixedUpdate(fabric::AppContext& ctx, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("character_movement");
    auto* inputManager = ctx.inputManager;

    // Camera direction from previous frame (CameraGameSystem runs in Update phase)
    fabric::Vec3f camFwd(0.0f, 0.0f, -1.0f);
    fabric::Vec3f camRight(1.0f, 0.0f, 0.0f);

    if (camera_) {
        camFwd = camera_->forward();
        camRight = camera_->right();
    }

    if (movementFSM_.isFlying()) {
        auto fwd = camFwd;
        auto right = camRight;

        fabric::Vec3f moveDir(0.0f, 0.0f, 0.0f);
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_FORWARD))
            moveDir = moveDir + fwd;
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_BACKWARD))
            moveDir = moveDir - fwd;
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_RIGHT))
            moveDir = moveDir + right;
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_LEFT))
            moveDir = moveDir - right;
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_UP))
            moveDir = moveDir + fabric::Vec3f(0.0f, 1.0f, 0.0f);
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_DOWN))
            moveDir = moveDir - fabric::Vec3f(0.0f, 1.0f, 0.0f);

        float len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
        if (len > 0.001f)
            moveDir = fabric::Vec3f(moveDir.x / len, moveDir.y / len, moveDir.z / len);

        // Determine speed: base flight/noclip speed, with LCTRL multiplier
        float baseSpeed = movementFSM_.isNoclip() ? charConfig_.noclipSpeed : charConfig_.flightSpeed;
        if (inputManager->isActionActive(recurse::input::K_ACTION_SPEED_BOOST))
            baseSpeed *= charConfig_.speedMultiplier;

        fabric::Vec3f displacement(moveDir.x * baseSpeed * fixedDt, moveDir.y * baseSpeed * fixedDt,
                                   moveDir.z * baseSpeed * fixedDt);

        if (movementFSM_.isNoclip()) {
            // Noclip: no collision, direct position update
            playerPos_ = fabric::Vec3f(playerPos_.x + displacement.x, playerPos_.y + displacement.y,
                                       playerPos_.z + displacement.z);
            syncPlayerPositionViews(playerPosD_, playerPos_, playerPos_);
        } else {
            // Flying: with collision
            auto result = flightCtrl_->move(playerPos_, displacement, voxelSim_->simulationGrid());
            syncPlayerPositionViews(playerPosD_, playerPos_, result.resolvedPosition);
        }

    } else {
        // Ground mode: flatten forward/right to XZ plane
        auto fwd = camFwd;
        auto right = camRight;

        fabric::Vec3f flatFwd(fwd.x, 0.0f, fwd.z);
        float fwdLen = std::sqrt(flatFwd.x * flatFwd.x + flatFwd.z * flatFwd.z);
        if (fwdLen > 0.001f)
            flatFwd = fabric::Vec3f(flatFwd.x / fwdLen, 0.0f, flatFwd.z / fwdLen);

        fabric::Vec3f flatRight(right.x, 0.0f, right.z);
        float rightLen = std::sqrt(flatRight.x * flatRight.x + flatRight.z * flatRight.z);
        if (rightLen > 0.001f)
            flatRight = fabric::Vec3f(flatRight.x / rightLen, 0.0f, flatRight.z / rightLen);

        fabric::Vec3f horizMove(0.0f, 0.0f, 0.0f);
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_FORWARD))
            horizMove = horizMove + flatFwd;
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_BACKWARD))
            horizMove = horizMove - flatFwd;
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_RIGHT))
            horizMove = horizMove + flatRight;
        if (inputManager->isActionActive(recurse::input::K_ACTION_MOVE_LEFT))
            horizMove = horizMove - flatRight;

        float horizLen = std::sqrt(horizMove.x * horizMove.x + horizMove.z * horizMove.z);
        if (horizLen > 0.001f)
            horizMove = fabric::Vec3f(horizMove.x / horizLen, 0.0f, horizMove.z / horizLen);

        // Gravity
        playerVel_.y -= charConfig_.gravity * fixedDt;

        fabric::Vec3f velocity(horizMove.x * charConfig_.walkSpeed, playerVel_.y, horizMove.z * charConfig_.walkSpeed);

        // Jolt controller required for ground movement
        if (!joltCharCtrl_ || !physics_ || !physics_->physicsWorld().tempAllocator())
            return;

        auto result = joltCharCtrl_->move(playerPos_, velocity, fixedDt, *physics_->physicsWorld().tempAllocator());
        syncPlayerPositionViews(playerPosD_, playerPos_, result.resolvedPosition);

        // Update velocity from collision response
        playerVel_.y = result.velocity.y;

        if (result.onGround) {
            playerVel_.y = 0.0f;
            if (movementFSM_.currentState() == CharacterState::Falling ||
                movementFSM_.currentState() == CharacterState::Jumping) {
                auto prev = movementFSM_.currentState();
                if (movementFSM_.tryTransition(CharacterState::Grounded)) {
                    FABRIC_LOG_DEBUG("Movement state: {} -> Grounded (landed)", MovementFSM::stateToString(prev));
                }
            }
        } else if (movementFSM_.isGrounded() || movementFSM_.currentState() == CharacterState::Jumping) {
            auto prev = movementFSM_.currentState();
            if (movementFSM_.tryTransition(CharacterState::Falling)) {
                FABRIC_LOG_DEBUG("Movement state: {} -> Falling (left ground)", MovementFSM::stateToString(prev));
            }
        }

        // Ceiling collision: kill upward velocity
        if (result.hitY && playerVel_.y > 0.0f)
            playerVel_.y = 0.0f;
    }

    if (playerEntity_.is_valid())
        playerEntity_.set<fabric::Position>({playerPos_.x, playerPos_.y, playerPos_.z});
}

void CharacterMovementSystem::onWorldBegin() {
    if (physics_ && physics_->physicsWorld().initialized()) {
        JoltCharacterConfig config;
        config.width = K_CHAR_WIDTH;
        config.height = K_CHAR_HEIGHT;
        config.mass = 70.0f;
        config.maxSlopeAngle = charConfig_.slopeLimit;
        joltCharCtrl_ = physics_->physicsWorld().createCharacter(config);
        if (joltCharCtrl_)
            joltCharCtrl_->setPosition(playerPos_);
    }

    if (ecsWorld_) {
        playerEntity_ = ecsWorld_->entity("player")
                            .add<fabric::WorldScoped>()
                            .set<fabric::Position>({playerPos_.x, playerPos_.y, playerPos_.z})
                            .set<recurse::StreamSource>({streamRadius_, collisionRadius_});
    }
}

void CharacterMovementSystem::onWorldEnd() {
    movementFSM_ = MovementFSM{};
    playerVel_ = {};
    joltCharCtrl_ = nullptr;
    playerPos_ = {K_DEFAULT_SPAWN_X, K_DEFAULT_SPAWN_Y, K_DEFAULT_SPAWN_Z};
    playerPosD_ = fabric::Vector3<double, fabric::Space::World>(static_cast<double>(K_DEFAULT_SPAWN_X),
                                                                static_cast<double>(K_DEFAULT_SPAWN_Y),
                                                                static_cast<double>(K_DEFAULT_SPAWN_Z));
    playerEntity_ = flecs::entity{};
}

void CharacterMovementSystem::configureDependencies() {
    after<PhysicsGameSystem>();
    after<TerrainSystem>();
    after<VoxelSimulationSystem>();
}

} // namespace recurse::systems

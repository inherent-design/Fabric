#include "recurse/physics/JoltCharacterController.hh"
#include "fabric/log/Log.hh"

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace recurse {

namespace {

// Convert fabric::Vec3f to JPH::RVec3
inline JPH::RVec3 toJolt(const fabric::Vec3f& v) {
    return JPH::RVec3(v.x, v.y, v.z);
}

// Convert JPH::RVec3 to fabric::Vec3f
inline fabric::Vec3f fromJolt(const JPH::RVec3& v) {
    return fabric::Vec3f(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()));
}

// Convert fabric::Vec3f to JPH::Vec3
inline JPH::Vec3 toJoltVec(const fabric::Vec3f& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

// Convert JPH::Vec3 to fabric::Vec3f
inline fabric::Vec3f fromJoltVec(const JPH::Vec3& v) {
    return fabric::Vec3f(v.GetX(), v.GetY(), v.GetZ());
}

} // namespace

JoltCharacterController::JoltCharacterController(JPH::PhysicsSystem* system, const JoltCharacterConfig& config)
    : physicsSystem_(system) {
    if (!system) {
        FABRIC_LOG_ERROR("JoltCharacterController: physics system is null");
        return;
    }

    // Create a capsule shape standing upright (Y-up)
    // Capsule: cylinder with hemispherical caps
    // Height here is the cylinder portion, total height = height + width (radius * 2)
    float radius = config.width * 0.5f;
    float cylinderHeight = config.height - config.width; // Subtract cap radii

    JPH::RefConst<JPH::Shape> capsuleShape = new JPH::CapsuleShape(cylinderHeight * 0.5f, radius);

    // CharacterVirtual settings
    JPH::CharacterVirtualSettings settings;
    settings.mShape = capsuleShape;
    settings.mMass = config.mass;
    settings.mMaxSlopeAngle = config.maxSlopeAngle;
    settings.mMaxStrength = config.maxStrength;
    settings.mCharacterPadding = config.characterPadding;
    settings.mPenetrationRecoverySpeed = config.penetrationRecoverySpeed;

    // Initial position at origin (caller sets via setPosition)
    character_ =
        std::make_unique<JPH::CharacterVirtual>(&settings, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0, system);

    FABRIC_LOG_DEBUG("JoltCharacterController created: width={} height={} mass={}", config.width, config.height,
                     config.mass);
}

JoltCharacterController::~JoltCharacterController() = default;

JoltCharacterController::JoltCharacterController(JoltCharacterController&& other) noexcept
    : character_(std::move(other.character_)), physicsSystem_(other.physicsSystem_), wasOnGround_(other.wasOnGround_) {
    other.physicsSystem_ = nullptr;
}

JoltCharacterController& JoltCharacterController::operator=(JoltCharacterController&& other) noexcept {
    if (this != &other) {
        character_ = std::move(other.character_);
        physicsSystem_ = other.physicsSystem_;
        wasOnGround_ = other.wasOnGround_;
        other.physicsSystem_ = nullptr;
    }
    return *this;
}

JoltCharacterController::CollisionResult JoltCharacterController::move(const fabric::Vec3f& currentPos,
                                                                       const fabric::Vec3f& velocity, float deltaTime,
                                                                       JPH::TempAllocator& allocator) {
    CollisionResult result;

    if (!character_ || !physicsSystem_) {
        result.resolvedPosition = currentPos;
        result.velocity = velocity;
        return result;
    }

    // Update character position
    character_->SetPosition(toJolt(currentPos));
    character_->SetLinearVelocity(toJoltVec(velocity));

    // Use default filters - they allow collision with all layers by default
    JPH::BroadPhaseLayerFilter bpFilter;
    JPH::ObjectLayerFilter objectFilter;
    JPH::BodyFilter bodyFilter;
    JPH::ShapeFilter shapeFilter;

    // Update character collision
    // Gravity vector for character physics (Y-down, magnitude doesn't matter for collision)
    character_->Update(deltaTime, JPH::Vec3(0.0f, -20.0f, 0.0f), bpFilter, objectFilter, bodyFilter, shapeFilter,
                       allocator);

    // Extract results
    result.resolvedPosition = fromJolt(character_->GetPosition());
    result.velocity = fromJoltVec(character_->GetLinearVelocity());

    // Determine ground state
    JPH::CharacterVirtual::EGroundState groundState = character_->GetGroundState();
    result.onGround = (groundState == JPH::CharacterVirtual::EGroundState::OnGround);

    FABRIC_LOG_TRACE(
        "JoltCharacterController: pos=({:.2f},{:.2f},{:.2f}) vel=({:.2f},{:.2f},{:.2f}) onGround={} groundState={}",
        result.resolvedPosition.x, result.resolvedPosition.y, result.resolvedPosition.z, result.velocity.x,
        result.velocity.y, result.velocity.z, result.onGround, static_cast<int>(groundState));

    // Detect Y-axis collision by comparing input/output velocity
    // If we wanted to move on Y but got stopped or significantly slowed, we hit something
    if (std::abs(velocity.y) > 0.01f) {
        float velDiff = std::abs(velocity.y - result.velocity.y);
        result.hitY = velDiff > std::abs(velocity.y) * 0.5f;
    }

    // hitAny is true if we're blocked from our intended direction
    auto intendedPos = currentPos + velocity * deltaTime;
    auto actualDelta = result.resolvedPosition - currentPos;
    float intendedDist = (intendedPos - currentPos).length();
    float actualDist = actualDelta.length();
    result.hitAny = (intendedDist > 0.001f && actualDist < intendedDist * 0.99f);

    wasOnGround_ = result.onGround;

    return result;
}

bool JoltCharacterController::isOnGround() const {
    if (!character_)
        return false;
    return character_->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
}

bool JoltCharacterController::canWalkStairs() const {
    if (!character_)
        return false;
    // Check if character can walk stairs based on current velocity
    return isOnGround() && character_->CanWalkStairs(character_->GetLinearVelocity());
}

fabric::Vec3f JoltCharacterController::getPosition() const {
    if (!character_)
        return fabric::Vec3f(0.0f, 0.0f, 0.0f);
    return fromJolt(character_->GetPosition());
}

void JoltCharacterController::setPosition(const fabric::Vec3f& pos) {
    if (character_)
        character_->SetPosition(toJolt(pos));
}

fabric::Vec3f JoltCharacterController::getLinearVelocity() const {
    if (!character_)
        return fabric::Vec3f(0.0f, 0.0f, 0.0f);
    return fromJoltVec(character_->GetLinearVelocity());
}

void JoltCharacterController::setLinearVelocity(const fabric::Vec3f& vel) {
    if (character_)
        character_->SetLinearVelocity(toJoltVec(vel));
}

bool JoltCharacterController::setShape(const JPH::Shape* shape, float maxPenetration, JPH::TempAllocator& allocator) {
    if (!character_ || !shape)
        return false;

    JPH::BroadPhaseLayerFilter bpFilter;
    JPH::ObjectLayerFilter objectFilter;
    JPH::BodyFilter bodyFilter;
    JPH::ShapeFilter shapeFilter;

    return character_->SetShape(shape, maxPenetration, bpFilter, objectFilter, bodyFilter, shapeFilter, allocator);
}

} // namespace recurse

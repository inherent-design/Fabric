#include "fabric/core/PhysicsWorld.hh"
#include "fabric/core/ChunkedGrid.hh"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <cmath>
#include <gtest/gtest.h>

using namespace fabric;

// Lifecycle tests

TEST(PhysicsWorldTest, Instantiation) {
    PhysicsWorld pw;
    EXPECT_FALSE(pw.initialized());
}

TEST(PhysicsWorldTest, InitShutdown) {
    PhysicsWorld pw;
    pw.init();
    EXPECT_TRUE(pw.initialized());
    pw.shutdown();
    EXPECT_FALSE(pw.initialized());
}

TEST(PhysicsWorldTest, DoubleInitIsNoop) {
    PhysicsWorld pw;
    pw.init();
    pw.init(); // should not crash
    EXPECT_TRUE(pw.initialized());
    pw.shutdown();
}

TEST(PhysicsWorldTest, DoubleShutdownIsNoop) {
    PhysicsWorld pw;
    pw.init();
    pw.shutdown();
    pw.shutdown(); // should not crash
    EXPECT_FALSE(pw.initialized());
}

TEST(PhysicsWorldTest, DestructorCleansUp) {
    {
        PhysicsWorld pw;
        pw.init();
        // destructor should call shutdown
    }
    SUCCEED();
}

// Jolt system access

TEST(PhysicsWorldTest, JoltSystemAccessible) {
    PhysicsWorld pw;
    EXPECT_EQ(pw.joltSystem(), nullptr);
    pw.init();
    EXPECT_NE(pw.joltSystem(), nullptr);
    pw.shutdown();
    EXPECT_EQ(pw.joltSystem(), nullptr);
}

// Static body tests

TEST(PhysicsWorldTest, CreateStaticBody) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::BoxShape> box = new JPH::BoxShape(JPH::Vec3(1.0f, 1.0f, 1.0f));
    auto handle = pw.createStaticBody(box.GetPtr(), 0.0f, 5.0f, 0.0f);
    EXPECT_TRUE(handle.valid());

    pw.shutdown();
}

TEST(PhysicsWorldTest, CreateStaticBodyNullShape) {
    PhysicsWorld pw;
    pw.init();

    auto handle = pw.createStaticBody(nullptr, 0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(handle.valid());

    pw.shutdown();
}

TEST(PhysicsWorldTest, CreateStaticBodyBeforeInit) {
    PhysicsWorld pw;
    JPH::Ref<JPH::BoxShape> box = new JPH::BoxShape(JPH::Vec3(1.0f, 1.0f, 1.0f));
    auto handle = pw.createStaticBody(box.GetPtr(), 0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(handle.valid());
}

// Dynamic body tests

TEST(PhysicsWorldTest, CreateDynamicBody) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 10.0f, 0.0f, 5.0f);
    EXPECT_TRUE(handle.valid());

    pw.shutdown();
}

TEST(PhysicsWorldTest, CreateDynamicBodyDefaultMass) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::BoxShape> box = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    auto handle = pw.createDynamicBody(box.GetPtr(), 1.0f, 1.0f, 1.0f);
    EXPECT_TRUE(handle.valid());

    pw.shutdown();
}

// Body removal

TEST(PhysicsWorldTest, RemoveBody) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::BoxShape> box = new JPH::BoxShape(JPH::Vec3(1.0f, 1.0f, 1.0f));
    auto handle = pw.createStaticBody(box.GetPtr(), 0.0f, 0.0f, 0.0f);
    EXPECT_TRUE(handle.valid());

    pw.removeBody(handle);
    // Should not crash on double-remove
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, RemoveDynamicBody) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 10.0f, 0.0f, 1.0f);
    pw.removeBody(handle);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, RemoveInvalidHandle) {
    PhysicsWorld pw;
    pw.init();

    BodyHandle invalid{JPH::BodyID()};
    pw.removeBody(invalid); // should not crash
    SUCCEED();

    pw.shutdown();
}

// Step simulation

TEST(PhysicsWorldTest, StepEmptyWorld) {
    PhysicsWorld pw;
    pw.init();
    pw.step(1.0f / 60.0f);
    SUCCEED();
    pw.shutdown();
}

TEST(PhysicsWorldTest, StepWithBodies) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::BoxShape> floor = new JPH::BoxShape(JPH::Vec3(50.0f, 1.0f, 50.0f));
    pw.createStaticBody(floor.GetPtr(), 0.0f, -1.0f, 0.0f);

    JPH::Ref<JPH::SphereShape> ball = new JPH::SphereShape(0.5f);
    pw.createDynamicBody(ball.GetPtr(), 0.0f, 10.0f, 0.0f, 1.0f);

    // Step several frames
    for (int i = 0; i < 10; ++i) {
        pw.step(1.0f / 60.0f);
    }
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, StepZeroDt) {
    PhysicsWorld pw;
    pw.init();
    pw.step(0.0f); // should be a no-op
    SUCCEED();
    pw.shutdown();
}

TEST(PhysicsWorldTest, StepNegativeDt) {
    PhysicsWorld pw;
    pw.init();
    pw.step(-1.0f); // should be a no-op
    SUCCEED();
    pw.shutdown();
}

TEST(PhysicsWorldTest, StepBeforeInit) {
    PhysicsWorld pw;
    pw.step(1.0f / 60.0f); // should not crash
    SUCCEED();
}

// Density bridge

TEST(PhysicsWorldTest, RebuildChunkCollisionEmpty) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    // All zeros, no solid voxels
    pw.rebuildChunkCollision(grid, 0, 0, 0);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, RebuildChunkCollisionSolid) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    // Fill a small region with solid density
    for (int z = 0; z < 4; ++z) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                grid.set(x, y, z, 1.0f);
            }
        }
    }

    pw.rebuildChunkCollision(grid, 0, 0, 0);

    // Step to verify bodies work with the simulation
    pw.step(1.0f / 60.0f);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, RebuildChunkCollisionReplace) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(0, 0, 0, 1.0f);
    pw.rebuildChunkCollision(grid, 0, 0, 0);

    // Rebuild same chunk with different data (replaces old bodies)
    grid.set(1, 1, 1, 1.0f);
    pw.rebuildChunkCollision(grid, 0, 0, 0);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, RemoveChunkCollision) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(0, 0, 0, 1.0f);
    pw.rebuildChunkCollision(grid, 0, 0, 0);
    pw.removeChunkCollision(0, 0, 0);

    // Removing again should be safe
    pw.removeChunkCollision(0, 0, 0);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, RebuildChunkCollisionCustomThreshold) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(0, 0, 0, 0.3f);
    grid.set(1, 0, 0, 0.8f);

    // With threshold 0.5, only (1,0,0) should become solid
    pw.rebuildChunkCollision(grid, 0, 0, 0, 0.5f);

    // With threshold 0.2, both should be solid
    pw.rebuildChunkCollision(grid, 0, 0, 0, 0.2f);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, DensityBridgeWithDynamicBody) {
    PhysicsWorld pw;
    pw.init();

    // Create terrain from density
    ChunkedGrid<float> grid;
    for (int z = 0; z < 8; ++z) {
        for (int x = 0; x < 8; ++x) {
            grid.set(x, 0, z, 1.0f);
        }
    }
    pw.rebuildChunkCollision(grid, 0, 0, 0);

    // Drop a dynamic body onto the terrain
    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    pw.createDynamicBody(sphere.GetPtr(), 4.0f, 5.0f, 4.0f, 1.0f);

    for (int i = 0; i < 20; ++i) {
        pw.step(1.0f / 60.0f);
    }
    SUCCEED();

    pw.shutdown();
}

// Contact callback

TEST(PhysicsWorldTest, ContactCallback) {
    PhysicsWorld pw;
    pw.init();

    int contactCount = 0;
    pw.setContactCallback([&](const ContactEvent&) { ++contactCount; });

    // Create floor and falling ball
    JPH::Ref<JPH::BoxShape> floor = new JPH::BoxShape(JPH::Vec3(50.0f, 1.0f, 50.0f));
    pw.createStaticBody(floor.GetPtr(), 0.0f, -1.0f, 0.0f);

    JPH::Ref<JPH::SphereShape> ball = new JPH::SphereShape(0.5f);
    pw.createDynamicBody(ball.GetPtr(), 0.0f, 2.0f, 0.0f, 1.0f);

    // Step until contact occurs
    for (int i = 0; i < 120; ++i) {
        pw.step(1.0f / 60.0f);
    }
    // The ball should have hit the floor at some point
    EXPECT_GT(contactCount, 0);

    pw.shutdown();
}

// Multiple bodies

TEST(PhysicsWorldTest, MultipleBodies) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::BoxShape> box = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    std::vector<BodyHandle> handles;
    for (int i = 0; i < 16; ++i) {
        auto h = pw.createDynamicBody(box.GetPtr(), static_cast<float>(i), 10.0f, 0.0f, 1.0f);
        EXPECT_TRUE(h.valid());
        handles.push_back(h);
    }

    pw.step(1.0f / 60.0f);

    for (auto& h : handles) {
        pw.removeBody(h);
    }

    pw.shutdown();
}

// Force and impulse API

TEST(PhysicsWorldTest, ApplyForceMovesBody) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 0.0f, 0.0f, 1.0f);

    auto posBefore = pw.getBodyPosition(handle);

    for (int i = 0; i < 10; ++i) {
        pw.applyForce(handle, 0.0f, 1000.0f, 0.0f);
        pw.step(1.0f / 60.0f);
    }

    auto posAfter = pw.getBodyPosition(handle);
    EXPECT_GT(posAfter.y, posBefore.y);

    pw.shutdown();
}

TEST(PhysicsWorldTest, ApplyImpulseChangesVelocity) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 0.0f, 0.0f, 1.0f);

    pw.applyImpulse(handle, 10.0f, 0.0f, 0.0f);

    auto vel = pw.getLinearVelocity(handle);
    EXPECT_GT(vel.x, 0.0f);

    pw.shutdown();
}

TEST(PhysicsWorldTest, ApplyTorqueNoCrash) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 5.0f, 0.0f, 1.0f);

    pw.applyTorque(handle, 0.0f, 10.0f, 0.0f);
    pw.step(1.0f / 60.0f);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, SetAndGetLinearVelocity) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 0.0f, 0.0f, 1.0f);

    pw.setLinearVelocity(handle, 5.0f, 0.0f, -3.0f);
    auto vel = pw.getLinearVelocity(handle);
    EXPECT_NEAR(vel.x, 5.0f, 0.01f);
    EXPECT_NEAR(vel.z, -3.0f, 0.01f);

    pw.shutdown();
}

// Body property setters

TEST(PhysicsWorldTest, SetFrictionAndRestitution) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 5.0f, 0.0f, 1.0f);

    pw.setFriction(handle, 0.8f);
    pw.setRestitution(handle, 0.5f);
    pw.step(1.0f / 60.0f);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, SetDamping) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 5.0f, 0.0f, 1.0f);

    pw.setLinearDamping(handle, 0.5f);
    pw.setAngularDamping(handle, 0.3f);
    pw.step(1.0f / 60.0f);
    SUCCEED();

    pw.shutdown();
}

// Body position and rotation queries

TEST(PhysicsWorldTest, GetBodyPositionAtCreation) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 3.0f, 7.0f, -2.0f, 1.0f);

    auto pos = pw.getBodyPosition(handle);
    EXPECT_NEAR(pos.x, 3.0f, 0.01f);
    EXPECT_NEAR(pos.y, 7.0f, 0.01f);
    EXPECT_NEAR(pos.z, -2.0f, 0.01f);

    pw.shutdown();
}

TEST(PhysicsWorldTest, GetBodyPositionAfterStep) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto handle = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 10.0f, 0.0f, 1.0f);

    auto posBefore = pw.getBodyPosition(handle);
    for (int i = 0; i < 30; ++i)
        pw.step(1.0f / 60.0f);
    auto posAfter = pw.getBodyPosition(handle);

    // Gravity should pull the body down
    EXPECT_LT(posAfter.y, posBefore.y);

    pw.shutdown();
}

TEST(PhysicsWorldTest, GetBodyRotationIdentity) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::BoxShape> box = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    auto handle = pw.createDynamicBody(box.GetPtr(), 0.0f, 5.0f, 0.0f, 1.0f);

    auto rot = pw.getBodyRotation(handle);
    // Identity quaternion: (0, 0, 0, 1)
    EXPECT_NEAR(rot.x, 0.0f, 0.01f);
    EXPECT_NEAR(rot.y, 0.0f, 0.01f);
    EXPECT_NEAR(rot.z, 0.0f, 0.01f);
    EXPECT_NEAR(rot.w, 1.0f, 0.01f);

    pw.shutdown();
}

// Debris

TEST(PhysicsWorldTest, CreateDebris) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.25f);
    auto handle = pw.createDebris(sphere.GetPtr(), 0.0f, 5.0f, 0.0f, 1.0f, 2.0f, 0.0f, 1.0f);
    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(pw.debrisCount(), 1u);

    pw.shutdown();
}

TEST(PhysicsWorldTest, DebrisRemovedAfterLifetime) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.25f);
    pw.createDebris(sphere.GetPtr(), 0.0f, 5.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f);
    EXPECT_EQ(pw.debrisCount(), 1u);

    // Step for less than lifetime (10 * 1/60 = 0.167s < 0.5s)
    for (int i = 0; i < 10; ++i)
        pw.step(1.0f / 60.0f);
    EXPECT_EQ(pw.debrisCount(), 1u);

    // Step past lifetime (total ~0.667s > 0.5s)
    for (int i = 0; i < 30; ++i)
        pw.step(1.0f / 60.0f);
    EXPECT_EQ(pw.debrisCount(), 0u);

    pw.shutdown();
}

TEST(PhysicsWorldTest, DebrisHasInitialVelocity) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.25f);
    auto handle = pw.createDebris(sphere.GetPtr(), 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 5.0f);

    auto vel = pw.getLinearVelocity(handle);
    EXPECT_NEAR(vel.x, 10.0f, 0.1f);

    pw.shutdown();
}

TEST(PhysicsWorldTest, MultipleDebrisLifetimes) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.25f);
    pw.createDebris(sphere.GetPtr(), 0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f);
    pw.createDebris(sphere.GetPtr(), 1.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_EQ(pw.debrisCount(), 2u);

    // Step past first debris lifetime (~0.333s > 0.2s)
    for (int i = 0; i < 20; ++i)
        pw.step(1.0f / 60.0f);
    EXPECT_EQ(pw.debrisCount(), 1u);

    // Step past second debris lifetime (~1.333s > 1.0s)
    for (int i = 0; i < 60; ++i)
        pw.step(1.0f / 60.0f);
    EXPECT_EQ(pw.debrisCount(), 0u);

    pw.shutdown();
}

// Constraints

TEST(PhysicsWorldTest, CreateFixedConstraint) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto a = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 5.0f, 0.0f, 1.0f);
    auto b = pw.createDynamicBody(sphere.GetPtr(), 2.0f, 5.0f, 0.0f, 1.0f);

    auto constraint = pw.createFixedConstraint(a, b);
    EXPECT_TRUE(constraint.valid());

    for (int i = 0; i < 10; ++i)
        pw.step(1.0f / 60.0f);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, RemoveFixedConstraint) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto a = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 5.0f, 0.0f, 1.0f);
    auto b = pw.createDynamicBody(sphere.GetPtr(), 2.0f, 5.0f, 0.0f, 1.0f);

    auto constraint = pw.createFixedConstraint(a, b);
    pw.removeConstraint(constraint);

    // Bodies should now move independently
    pw.step(1.0f / 60.0f);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, RemoveInvalidConstraint) {
    PhysicsWorld pw;
    pw.init();

    ConstraintHandle invalid{0};
    pw.removeConstraint(invalid);

    ConstraintHandle nonExistent{999};
    pw.removeConstraint(nonExistent);
    SUCCEED();

    pw.shutdown();
}

TEST(PhysicsWorldTest, FixedConstraintKeepsBodiesTogether) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.5f);
    auto a = pw.createDynamicBody(sphere.GetPtr(), 0.0f, 10.0f, 0.0f, 1.0f);
    auto b = pw.createDynamicBody(sphere.GetPtr(), 1.0f, 10.0f, 0.0f, 1.0f);

    pw.createFixedConstraint(a, b);

    for (int i = 0; i < 60; ++i)
        pw.step(1.0f / 60.0f);

    auto posA = pw.getBodyPosition(a);
    auto posB = pw.getBodyPosition(b);

    // Distance between constrained bodies should remain approximately 1.0
    float dx = posA.x - posB.x;
    float dy = posA.y - posB.y;
    float dz = posA.z - posB.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    EXPECT_NEAR(dist, 1.0f, 0.2f);

    pw.shutdown();
}

// Edge cases for new APIs

TEST(PhysicsWorldTest, ForceOnInvalidHandle) {
    PhysicsWorld pw;
    pw.init();

    BodyHandle invalid{JPH::BodyID()};
    pw.applyForce(invalid, 1.0f, 0.0f, 0.0f);
    pw.applyImpulse(invalid, 1.0f, 0.0f, 0.0f);
    pw.applyTorque(invalid, 1.0f, 0.0f, 0.0f);
    pw.setLinearVelocity(invalid, 1.0f, 0.0f, 0.0f);

    auto vel = pw.getLinearVelocity(invalid);
    EXPECT_NEAR(vel.x, 0.0f, 0.01f);

    auto pos = pw.getBodyPosition(invalid);
    EXPECT_NEAR(pos.x, 0.0f, 0.01f);

    auto rot = pw.getBodyRotation(invalid);
    EXPECT_NEAR(rot.w, 1.0f, 0.01f);

    SUCCEED();
    pw.shutdown();
}

TEST(PhysicsWorldTest, DebrisNullShape) {
    PhysicsWorld pw;
    pw.init();

    auto handle = pw.createDebris(nullptr, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_FALSE(handle.valid());
    EXPECT_EQ(pw.debrisCount(), 0u);

    pw.shutdown();
}

TEST(PhysicsWorldTest, DebrisZeroLifetime) {
    PhysicsWorld pw;
    pw.init();

    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(0.25f);
    auto handle = pw.createDebris(sphere.GetPtr(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(handle.valid());
    EXPECT_EQ(pw.debrisCount(), 0u);

    pw.shutdown();
}

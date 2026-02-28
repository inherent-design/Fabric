#include "fabric/core/ECS.hh"
#include "fabric/core/Spatial.hh"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

using namespace fabric;

TEST(ECSTest, WorldCreation) {
    World world;
    // World should be valid after construction
    EXPECT_TRUE(world.get().get_world() != nullptr);
}

TEST(ECSTest, WorldMoveConstruction) {
    World original;
    original.registerCoreComponents();
    auto entity = original.get().entity("test_entity").set<Position>({1.0f, 2.0f, 3.0f});

    World moved(std::move(original));
    // Moved-to world should have the entity
    auto found = moved.get().lookup("test_entity");
    EXPECT_TRUE(found.is_valid());

    const auto* pos = found.try_get<Position>();
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
}

TEST(ECSTest, WorldMoveAssignment) {
    World a;
    a.registerCoreComponents();
    a.get().entity("a_entity").set<Position>({5.0f, 6.0f, 7.0f});

    World b;
    b = std::move(a);

    auto found = b.get().lookup("a_entity");
    EXPECT_TRUE(found.is_valid());
}

TEST(ECSTest, ComponentRegistration) {
    World world;
    world.registerCoreComponents();

    // Components should be queryable by name after registration
    auto posComp = world.get().lookup("Position");
    auto rotComp = world.get().lookup("Rotation");
    auto scaleComp = world.get().lookup("Scale");
    auto bbComp = world.get().lookup("BoundingBox");
    auto ltwComp = world.get().lookup("LocalToWorld");

    EXPECT_TRUE(posComp.is_valid());
    EXPECT_TRUE(rotComp.is_valid());
    EXPECT_TRUE(scaleComp.is_valid());
    EXPECT_TRUE(bbComp.is_valid());
    EXPECT_TRUE(ltwComp.is_valid());
}

TEST(ECSTest, EntityCreationWithComponents) {
    World world;
    world.registerCoreComponents();

    auto entity = world.get()
                      .entity("cube")
                      .set<Position>({1.0f, 2.0f, 3.0f})
                      .set<Rotation>({0.0f, 0.0f, 0.0f, 1.0f})
                      .set<Scale>({1.0f, 1.0f, 1.0f});

    EXPECT_TRUE(entity.has<Position>());
    EXPECT_TRUE(entity.has<Rotation>());
    EXPECT_TRUE(entity.has<Scale>());

    const auto* pos = entity.try_get<Position>();
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
    EXPECT_FLOAT_EQ(pos->y, 2.0f);
    EXPECT_FLOAT_EQ(pos->z, 3.0f);

    const auto* rot = entity.try_get<Rotation>();
    ASSERT_NE(rot, nullptr);
    EXPECT_FLOAT_EQ(rot->w, 1.0f);
}

TEST(ECSTest, EntityWithBoundingBox) {
    World world;
    world.registerCoreComponents();

    auto entity = world.get()
                      .entity()
                      .set<Position>({0.0f, 0.0f, 0.0f})
                      .set<BoundingBox>({-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f});

    const auto* bb = entity.try_get<BoundingBox>();
    ASSERT_NE(bb, nullptr);
    EXPECT_FLOAT_EQ(bb->minX, -1.0f);
    EXPECT_FLOAT_EQ(bb->maxX, 1.0f);
}

TEST(ECSTest, ChildOfRelationship) {
    World world;
    world.registerCoreComponents();

    auto parent = world.get().entity("parent").set<Position>({10.0f, 0.0f, 0.0f});

    auto child = world.get().entity("child").child_of(parent).set<Position>({1.0f, 0.0f, 0.0f});

    // Child should have ChildOf relationship to parent
    EXPECT_TRUE(child.has(flecs::ChildOf, parent));

    // Parent should be retrievable
    EXPECT_EQ(child.parent(), parent);

    // Iterate children of parent
    int childCount = 0;
    world.get().each([&](flecs::entity e, const Position&) {
        if (e.has(flecs::ChildOf, parent)) {
            childCount++;
        }
    });
    EXPECT_EQ(childCount, 1);
}

TEST(ECSTest, QueryIteration) {
    World world;
    world.registerCoreComponents();

    // Create several entities with Position
    for (int i = 0; i < 10; i++) {
        world.get().entity().set<Position>({static_cast<float>(i), 0.0f, 0.0f});
    }

    // Query all entities with Position
    int count = 0;
    float sumX = 0.0f;
    world.get().each([&](const Position& pos) {
        sumX += pos.x;
        count++;
    });

    EXPECT_EQ(count, 10);
    EXPECT_FLOAT_EQ(sumX, 45.0f); // 0+1+2+...+9
}

TEST(ECSTest, CascadeHierarchyOrdering) {
    World world;
    world.registerCoreComponents();

    auto root = world.get().entity("root").set<Position>({0.0f, 0.0f, 0.0f});

    auto childA = world.get().entity("childA").child_of(root).set<Position>({1.0f, 0.0f, 0.0f});

    auto grandchild = world.get().entity("grandchild").child_of(childA).set<Position>({2.0f, 0.0f, 0.0f});

    // CASCADE query ensures parents are processed before children
    auto query =
        world.get().query_builder<const Position>().with(flecs::ChildOf, flecs::Wildcard).cascade().optional().build();

    std::vector<std::string> order;
    query.each([&](flecs::entity e, const Position&) { order.push_back(std::string(e.name().c_str())); });

    // Root should appear before childA, childA before grandchild
    auto rootIdx = std::find(order.begin(), order.end(), "root");
    auto childIdx = std::find(order.begin(), order.end(), "childA");
    auto grandchildIdx = std::find(order.begin(), order.end(), "grandchild");

    ASSERT_NE(rootIdx, order.end());
    ASSERT_NE(childIdx, order.end());
    ASSERT_NE(grandchildIdx, order.end());
    EXPECT_LT(rootIdx, childIdx);
    EXPECT_LT(childIdx, grandchildIdx);
}

TEST(ECSTest, EntityDeletion) {
    World world;
    world.registerCoreComponents();

    auto entity = world.get().entity("doomed").set<Position>({1.0f, 2.0f, 3.0f});

    EXPECT_TRUE(entity.is_alive());

    entity.destruct();

    EXPECT_FALSE(entity.is_alive());
}

TEST(ECSTest, ComponentRemoval) {
    World world;
    world.registerCoreComponents();

    auto entity = world.get().entity().set<Position>({1.0f, 2.0f, 3.0f}).set<Scale>({1.0f, 1.0f, 1.0f});

    EXPECT_TRUE(entity.has<Position>());
    EXPECT_TRUE(entity.has<Scale>());

    entity.remove<Scale>();

    EXPECT_TRUE(entity.has<Position>());
    EXPECT_FALSE(entity.has<Scale>());
}

TEST(ECSTest, CascadeParentDeletion) {
    World world;
    world.registerCoreComponents();

    auto parent = world.get().entity("parent").set<Position>({0.0f, 0.0f, 0.0f});

    auto child = world.get().entity("child").child_of(parent).set<Position>({1.0f, 0.0f, 0.0f});

    auto grandchild = world.get().entity("gchild").child_of(child).set<Position>({2.0f, 0.0f, 0.0f});

    // Deleting parent should cascade to children
    parent.destruct();

    EXPECT_FALSE(parent.is_alive());
    EXPECT_FALSE(child.is_alive());
    EXPECT_FALSE(grandchild.is_alive());
}

TEST(ECSTest, Progress) {
    World world;
    world.registerCoreComponents();

    // Create entity and a system that modifies position
    world.get().entity().set<Position>({0.0f, 0.0f, 0.0f});

    int systemRan = 0;
    world.get().system<Position>("MoveSystem").each([&](Position& pos) {
        pos.x += 1.0f;
        systemRan++;
    });

    world.progress(1.0f / 60.0f);
    EXPECT_EQ(systemRan, 1);

    // Verify position was modified
    int count = 0;
    world.get().each([&](const Position& pos) {
        EXPECT_FLOAT_EQ(pos.x, 1.0f);
        count++;
    });
    EXPECT_EQ(count, 1);
}

TEST(ECSTest, CreateSceneEntity) {
    World world;
    world.registerCoreComponents();

    auto entity = world.createSceneEntity("cube");

    EXPECT_TRUE(entity.has<Position>());
    EXPECT_TRUE(entity.has<Rotation>());
    EXPECT_TRUE(entity.has<Scale>());
    EXPECT_TRUE(entity.has<LocalToWorld>());
    EXPECT_TRUE(entity.has<SceneEntity>());

    // Default position is origin
    const auto* pos = entity.try_get<Position>();
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 0.0f);
    EXPECT_FLOAT_EQ(pos->y, 0.0f);
    EXPECT_FLOAT_EQ(pos->z, 0.0f);

    // Default rotation is identity quaternion
    const auto* rot = entity.try_get<Rotation>();
    ASSERT_NE(rot, nullptr);
    EXPECT_FLOAT_EQ(rot->w, 1.0f);

    // Default scale is uniform 1
    const auto* scl = entity.try_get<Scale>();
    ASSERT_NE(scl, nullptr);
    EXPECT_FLOAT_EQ(scl->x, 1.0f);
    EXPECT_FLOAT_EQ(scl->y, 1.0f);
    EXPECT_FLOAT_EQ(scl->z, 1.0f);
}

TEST(ECSTest, CreateSceneEntityUnnamed) {
    World world;
    world.registerCoreComponents();

    auto entity = world.createSceneEntity();
    EXPECT_TRUE(entity.is_alive());
    EXPECT_TRUE(entity.has<SceneEntity>());
    EXPECT_TRUE(entity.has<Position>());
}

TEST(ECSTest, CreateChildEntityWithParent) {
    World world;
    world.registerCoreComponents();

    auto parent = world.createSceneEntity("parent");
    auto child = world.createChildEntity(parent, "child");

    EXPECT_TRUE(child.has<SceneEntity>());
    EXPECT_TRUE(child.has<Position>());
    EXPECT_TRUE(child.has(flecs::ChildOf, parent));
    EXPECT_EQ(child.parent(), parent);
}

TEST(ECSTest, SceneEntityComponentRegistration) {
    World world;
    world.registerCoreComponents();

    auto seComp = world.get().lookup("SceneEntity");
    auto rendComp = world.get().lookup("Renderable");

    EXPECT_TRUE(seComp.is_valid());
    EXPECT_TRUE(rendComp.is_valid());
}

// Helper: extract translation from column-major LocalToWorld matrix
static void extractTranslation(const LocalToWorld& ltw, float& x, float& y, float& z) {
    // Column-major: translation is at indices 12, 13, 14
    x = ltw.matrix[12];
    y = ltw.matrix[13];
    z = ltw.matrix[14];
}

TEST(ECSTest, UpdateTransformsRootEntity) {
    World world;
    world.registerCoreComponents();

    auto root = world.createSceneEntity("root");
    root.set<Position>({5.0f, 0.0f, 0.0f});

    world.updateTransforms();

    const auto* ltw = root.try_get<LocalToWorld>();
    ASSERT_NE(ltw, nullptr);
    float x, y, z;
    extractTranslation(*ltw, x, y, z);
    EXPECT_FLOAT_EQ(x, 5.0f);
    EXPECT_FLOAT_EQ(y, 0.0f);
    EXPECT_FLOAT_EQ(z, 0.0f);
}

TEST(ECSTest, UpdateTransformsParentChild) {
    World world;
    world.registerCoreComponents();

    auto parent = world.createSceneEntity("parent");
    parent.set<Position>({5.0f, 0.0f, 0.0f});

    auto child = world.createChildEntity(parent, "child");
    child.set<Position>({0.0f, 3.0f, 0.0f});

    world.updateTransforms();

    // Child world position should be parent + child = (5, 3, 0)
    const auto* ltw = child.try_get<LocalToWorld>();
    ASSERT_NE(ltw, nullptr);
    float x, y, z;
    extractTranslation(*ltw, x, y, z);
    EXPECT_FLOAT_EQ(x, 5.0f);
    EXPECT_FLOAT_EQ(y, 3.0f);
    EXPECT_FLOAT_EQ(z, 0.0f);
}

TEST(ECSTest, UpdateTransformsThreeLevels) {
    World world;
    world.registerCoreComponents();

    auto grandparent = world.createSceneEntity("gp");
    grandparent.set<Position>({1.0f, 0.0f, 0.0f});

    auto parent = world.createChildEntity(grandparent, "p");
    parent.set<Position>({0.0f, 2.0f, 0.0f});

    auto child = world.createChildEntity(parent, "c");
    child.set<Position>({0.0f, 0.0f, 3.0f});

    world.updateTransforms();

    // Child world position: (1, 2, 3)
    const auto* ltw = child.try_get<LocalToWorld>();
    ASSERT_NE(ltw, nullptr);
    float x, y, z;
    extractTranslation(*ltw, x, y, z);
    EXPECT_FLOAT_EQ(x, 1.0f);
    EXPECT_FLOAT_EQ(y, 2.0f);
    EXPECT_FLOAT_EQ(z, 3.0f);
}

TEST(ECSTest, UpdateTransformsRotationPropagation) {
    World world;
    world.registerCoreComponents();

    // Parent rotated 90 degrees around Y axis
    auto parent = world.createSceneEntity("parent");
    auto q = Quaternion<float>::fromAxisAngle(Vector3<float, Space::World>(0.0f, 1.0f, 0.0f),
                                              static_cast<float>(std::numbers::pi / 2.0));
    parent.set<Rotation>({q.x, q.y, q.z, q.w});

    // Child at local position (1, 0, 0)
    auto child = world.createChildEntity(parent, "child");
    child.set<Position>({1.0f, 0.0f, 0.0f});

    world.updateTransforms();

    // 90 degrees Y rotation maps (1,0,0) -> (0,0,-1)
    const auto* ltw = child.try_get<LocalToWorld>();
    ASSERT_NE(ltw, nullptr);
    float x, y, z;
    extractTranslation(*ltw, x, y, z);
    EXPECT_NEAR(x, 0.0f, 1e-5f);
    EXPECT_NEAR(y, 0.0f, 1e-5f);
    EXPECT_NEAR(z, -1.0f, 1e-5f);
}

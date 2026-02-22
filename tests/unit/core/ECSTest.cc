#include "fabric/core/ECS.hh"

#include <gtest/gtest.h>
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
    auto entity = original.get().entity("test_entity")
        .set<Position>({1.0f, 2.0f, 3.0f});

    World moved(std::move(original));
    // Moved-to world should have the entity
    auto found = moved.get().lookup("test_entity");
    EXPECT_TRUE(found.is_valid());

    const auto* pos = found.get<Position>();
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

    EXPECT_TRUE(posComp.is_valid());
    EXPECT_TRUE(rotComp.is_valid());
    EXPECT_TRUE(scaleComp.is_valid());
    EXPECT_TRUE(bbComp.is_valid());
}

TEST(ECSTest, EntityCreationWithComponents) {
    World world;
    world.registerCoreComponents();

    auto entity = world.get().entity("cube")
        .set<Position>({1.0f, 2.0f, 3.0f})
        .set<Rotation>({0.0f, 0.0f, 0.0f, 1.0f})
        .set<Scale>({1.0f, 1.0f, 1.0f});

    EXPECT_TRUE(entity.has<Position>());
    EXPECT_TRUE(entity.has<Rotation>());
    EXPECT_TRUE(entity.has<Scale>());

    const auto* pos = entity.get<Position>();
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
    EXPECT_FLOAT_EQ(pos->y, 2.0f);
    EXPECT_FLOAT_EQ(pos->z, 3.0f);

    const auto* rot = entity.get<Rotation>();
    ASSERT_NE(rot, nullptr);
    EXPECT_FLOAT_EQ(rot->w, 1.0f);
}

TEST(ECSTest, EntityWithBoundingBox) {
    World world;
    world.registerCoreComponents();

    auto entity = world.get().entity()
        .set<Position>({0.0f, 0.0f, 0.0f})
        .set<BoundingBox>({-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f});

    const auto* bb = entity.get<BoundingBox>();
    ASSERT_NE(bb, nullptr);
    EXPECT_FLOAT_EQ(bb->minX, -1.0f);
    EXPECT_FLOAT_EQ(bb->maxX, 1.0f);
}

TEST(ECSTest, ChildOfRelationship) {
    World world;
    world.registerCoreComponents();

    auto parent = world.get().entity("parent")
        .set<Position>({10.0f, 0.0f, 0.0f});

    auto child = world.get().entity("child")
        .child_of(parent)
        .set<Position>({1.0f, 0.0f, 0.0f});

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
        world.get().entity()
            .set<Position>({static_cast<float>(i), 0.0f, 0.0f});
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

    auto root = world.get().entity("root")
        .set<Position>({0.0f, 0.0f, 0.0f});

    auto childA = world.get().entity("childA")
        .child_of(root)
        .set<Position>({1.0f, 0.0f, 0.0f});

    auto grandchild = world.get().entity("grandchild")
        .child_of(childA)
        .set<Position>({2.0f, 0.0f, 0.0f});

    // CASCADE query ensures parents are processed before children
    auto query = world.get().query_builder<const Position>()
        .with(flecs::ChildOf, flecs::Wildcard).cascade().optional()
        .build();

    std::vector<std::string> order;
    query.each([&](flecs::entity e, const Position&) {
        order.push_back(std::string(e.name().c_str()));
    });

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

    auto entity = world.get().entity("doomed")
        .set<Position>({1.0f, 2.0f, 3.0f});

    EXPECT_TRUE(entity.is_alive());

    entity.destruct();

    EXPECT_FALSE(entity.is_alive());
}

TEST(ECSTest, ComponentRemoval) {
    World world;
    world.registerCoreComponents();

    auto entity = world.get().entity()
        .set<Position>({1.0f, 2.0f, 3.0f})
        .set<Scale>({1.0f, 1.0f, 1.0f});

    EXPECT_TRUE(entity.has<Position>());
    EXPECT_TRUE(entity.has<Scale>());

    entity.remove<Scale>();

    EXPECT_TRUE(entity.has<Position>());
    EXPECT_FALSE(entity.has<Scale>());
}

TEST(ECSTest, CascadeParentDeletion) {
    World world;
    world.registerCoreComponents();

    auto parent = world.get().entity("parent")
        .set<Position>({0.0f, 0.0f, 0.0f});

    auto child = world.get().entity("child")
        .child_of(parent)
        .set<Position>({1.0f, 0.0f, 0.0f});

    auto grandchild = world.get().entity("gchild")
        .child_of(child)
        .set<Position>({2.0f, 0.0f, 0.0f});

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
    world.get().entity()
        .set<Position>({0.0f, 0.0f, 0.0f});

    int systemRan = 0;
    world.get().system<Position>("MoveSystem")
        .each([&](Position& pos) {
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

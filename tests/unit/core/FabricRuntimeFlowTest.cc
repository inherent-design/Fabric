#include "fabric/core/Camera.hh"
#include "fabric/core/ChunkMeshManager.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/VoxelInteraction.hh"

#include <gtest/gtest.h>

#include <unordered_map>
#include <unordered_set>

using namespace fabric;

namespace {
using Essence = Vector4<float, Space::World>;

BoundingBox chunkBounds(const ChunkCoord& coord) {
    return BoundingBox{
        static_cast<float>(coord.cx * kChunkSize),       static_cast<float>(coord.cy * kChunkSize),
        static_cast<float>(coord.cz * kChunkSize),       static_cast<float>((coord.cx + 1) * kChunkSize),
        static_cast<float>((coord.cy + 1) * kChunkSize), static_cast<float>((coord.cz + 1) * kChunkSize)};
}
} // namespace

TEST(FabricRuntimeFlowTest, StreamingEntityAndMeshLifecycleMatchesUnloadCleanup) {
    EventDispatcher dispatcher;
    ChunkedGrid<float> density;
    ChunkedGrid<Essence> essence;
    ChunkMeshManager meshManager(dispatcher, density, essence);

    World world;
    world.registerCoreComponents();

    std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash> chunkEntities;
    std::unordered_map<ChunkCoord, ChunkMesh, ChunkCoordHash> gpuMeshes;
    std::unordered_set<ChunkCoord, ChunkCoordHash> gpuUploadQueue;

    ChunkCoord coord{0, 0, 0};

    density.set(0, 0, 0, 1.0f);
    essence.set(0, 0, 0, Essence(1.0f, 1.0f, 1.0f, 1.0f));

    meshManager.markDirty(coord.cx, coord.cy, coord.cz);
    gpuUploadQueue.insert(coord);

    auto ent = world.get().entity().add<SceneEntity>().set<BoundingBox>(chunkBounds(coord));
    chunkEntities[coord] = ent;

    EXPECT_EQ(meshManager.update(), 1);
    EXPECT_FALSE(meshManager.isDirty(coord));

    ChunkMesh placeholder;
    placeholder.valid = true;
    placeholder.indexCount = 36;
    gpuMeshes[coord] = placeholder;

    gpuUploadQueue.erase(coord);
    meshManager.removeChunk(coord);

    if (auto it = chunkEntities.find(coord); it != chunkEntities.end()) {
        it->second.destruct();
        chunkEntities.erase(it);
    }
    if (auto it = gpuMeshes.find(coord); it != gpuMeshes.end()) {
        gpuMeshes.erase(it);
    }
    density.removeChunk(coord.cx, coord.cy, coord.cz);
    essence.removeChunk(coord.cx, coord.cy, coord.cz);

    EXPECT_TRUE(gpuUploadQueue.empty());
    EXPECT_EQ(meshManager.meshFor(coord), nullptr);
    EXPECT_EQ(chunkEntities.find(coord), chunkEntities.end());
    EXPECT_EQ(gpuMeshes.find(coord), gpuMeshes.end());
    EXPECT_FALSE(density.hasChunk(coord.cx, coord.cy, coord.cz));
    EXPECT_FALSE(essence.hasChunk(coord.cx, coord.cy, coord.cz));
}

TEST(FabricRuntimeFlowTest, UploadQueueDropsEntryWhenChunkEntityNoLongerExists) {
    EventDispatcher dispatcher;
    ChunkedGrid<float> density;
    ChunkedGrid<Essence> essence;
    ChunkMeshManager meshManager(dispatcher, density, essence);

    std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash> chunkEntities;
    std::unordered_set<ChunkCoord, ChunkCoordHash> gpuUploadQueue;

    ChunkCoord coord{1, 0, 0};

    density.set(coord.cx * kChunkSize, 0, 0, 1.0f);
    meshManager.markDirty(coord.cx, coord.cy, coord.cz);
    EXPECT_EQ(meshManager.update(), 1);
    EXPECT_FALSE(meshManager.isDirty(coord));

    gpuUploadQueue.insert(coord);

    auto it = gpuUploadQueue.begin();
    while (it != gpuUploadQueue.end()) {
        if (chunkEntities.find(*it) == chunkEntities.end()) {
            it = gpuUploadQueue.erase(it);
            continue;
        }

        if (!meshManager.isDirty(*it)) {
            it = gpuUploadQueue.erase(it);
        } else {
            ++it;
        }
    }

    EXPECT_TRUE(gpuUploadQueue.empty());
}

TEST(FabricRuntimeFlowTest, VoxelChangedEventDrivesChunkDirtyAndRemeshFlow) {
    DensityField density;
    EssenceField essence;
    EventDispatcher dispatcher;
    VoxelInteraction interaction(density, essence, dispatcher);

    ChunkMeshManager meshManager(dispatcher, density.grid(), essence.grid());

    density.write(5, 5, 5, 1.0f);

    auto result = interaction.destroyMatterAt(density.grid(), 5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    ASSERT_TRUE(result.success);

    ChunkCoord changed{result.cx, result.cy, result.cz};
    EXPECT_TRUE(meshManager.isDirty(changed));

    int remeshed = meshManager.update();
    EXPECT_GE(remeshed, 1);
    EXPECT_FALSE(meshManager.isDirty(changed));
    EXPECT_NE(meshManager.meshFor(changed), nullptr);
}

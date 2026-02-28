#include "fabric/core/MeshLoader.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <fstream>
#include <gtest/gtest.h>
#include <ozz/animation/runtime/skeleton.h>

using namespace fabric;

class MeshLoaderTest : public ::testing::Test {
  protected:
    MeshLoader loader;
};

TEST_F(MeshLoaderTest, MeshDataDefaultsAreEmpty) {
    MeshData data;
    EXPECT_TRUE(data.positions.empty());
    EXPECT_TRUE(data.normals.empty());
    EXPECT_TRUE(data.uvs.empty());
    EXPECT_TRUE(data.indices.empty());
    EXPECT_TRUE(data.jointIndices.empty());
    EXPECT_TRUE(data.jointWeights.empty());
    EXPECT_TRUE(data.skeleton.empty());
}

TEST_F(MeshLoaderTest, MeshDataIdIsNonZero) {
    MeshData data;
    EXPECT_NE(data.id, 0u);
}

TEST_F(MeshLoaderTest, TwoMeshDataGetDistinctIds) {
    MeshData a;
    MeshData b;
    EXPECT_NE(a.id, b.id);
}

TEST_F(MeshLoaderTest, MeshDataIdSurvivesMove) {
    MeshData original;
    original.positions.resize(10);
    const auto expectedId = original.id;
    MeshData moved(std::move(original));
    EXPECT_EQ(moved.id, expectedId);
}

TEST_F(MeshLoaderTest, MeshDataIdSurvivesMoveAssignment) {
    MeshData original;
    const auto expectedId = original.id;
    MeshData target;
    target = std::move(original);
    EXPECT_EQ(target.id, expectedId);
}

TEST_F(MeshLoaderTest, JointInfoDefaultValues) {
    JointInfo joint;
    EXPECT_TRUE(joint.name.empty());
    EXPECT_EQ(joint.parentIndex, -1);
    // Identity matrix
    EXPECT_FLOAT_EQ(joint.inverseBindMatrix[0], 1.0f);
    EXPECT_FLOAT_EQ(joint.inverseBindMatrix[5], 1.0f);
    EXPECT_FLOAT_EQ(joint.inverseBindMatrix[10], 1.0f);
    EXPECT_FLOAT_EQ(joint.inverseBindMatrix[15], 1.0f);
    EXPECT_FLOAT_EQ(joint.inverseBindMatrix[1], 0.0f);
}

TEST_F(MeshLoaderTest, MissingFileThrows) {
    EXPECT_THROW(loader.load("/nonexistent/path/to/mesh.glb"), FabricException);
}

TEST_F(MeshLoaderTest, InvalidFileThrows) {
    // Create a temporary invalid file
    auto tmpDir = std::filesystem::temp_directory_path();
    auto tmpFile = tmpDir / "fabric_test_invalid.glb";
    {
        std::ofstream out(tmpFile, std::ios::binary);
        out << "not a valid gltf file";
    }
    EXPECT_THROW(loader.load(tmpFile), FabricException);
    std::filesystem::remove(tmpFile);
}

TEST_F(MeshLoaderTest, MeshDataVectorSizesConsistent) {
    // When joint data is present, indices and weights should match vertex count
    MeshData data;
    data.positions.resize(100);
    data.jointIndices.resize(100);
    data.jointWeights.resize(100);
    EXPECT_EQ(data.positions.size(), data.jointIndices.size());
    EXPECT_EQ(data.positions.size(), data.jointWeights.size());
}

TEST_F(MeshLoaderTest, SkeletonCanHold100Joints) {
    MeshData data;
    data.skeleton.resize(100);
    for (int i = 0; i < 100; ++i) {
        data.skeleton[static_cast<size_t>(i)].name = "joint_" + std::to_string(i);
        data.skeleton[static_cast<size_t>(i)].parentIndex = i > 0 ? i - 1 : -1;
    }
    EXPECT_EQ(data.skeleton.size(), 100u);
    EXPECT_EQ(data.skeleton[0].parentIndex, -1);
    EXPECT_EQ(data.skeleton[99].parentIndex, 98);
}

TEST_F(MeshLoaderTest, BuildOzzSkeletonFromJointInfo) {
    std::vector<JointInfo> joints(3);
    joints[0].name = "root";
    joints[0].parentIndex = -1;
    joints[1].name = "spine";
    joints[1].parentIndex = 0;
    joints[2].name = "head";
    joints[2].parentIndex = 1;

    auto skeleton = buildOzzSkeleton(joints);
    ASSERT_NE(skeleton, nullptr);
    EXPECT_EQ(skeleton->num_joints(), 3);
}

TEST_F(MeshLoaderTest, BuildOzzSkeletonEmpty) {
    std::vector<JointInfo> empty;
    auto skeleton = buildOzzSkeleton(empty);
    EXPECT_EQ(skeleton, nullptr);
}

TEST_F(MeshLoaderTest, SyntheticSkinnedMeshEndToEnd) {
    MeshData mesh;
    mesh.positions.resize(4);
    mesh.indices = {0, 1, 2, 0, 2, 3};

    mesh.skeleton.resize(2);
    mesh.skeleton[0].name = "root";
    mesh.skeleton[0].parentIndex = -1;
    mesh.skeleton[1].name = "child";
    mesh.skeleton[1].parentIndex = 0;

    auto skeleton = buildOzzSkeleton(mesh.skeleton);
    ASSERT_NE(skeleton, nullptr);
    EXPECT_EQ(skeleton->num_joints(), 2);
    EXPECT_GT(skeleton->num_soa_joints(), 0);
}

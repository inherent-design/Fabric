#include "fabric/core/JsonTypes.hh"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace fabric {

class JsonTypesTest : public ::testing::Test {};

TEST_F(JsonTypesTest, Vector2RoundTrip) {
    Vector2<float, Space::World> original(3.5f, -1.2f);
    nlohmann::json j = original;
    auto restored = j.get<Vector2<float, Space::World>>();
    EXPECT_FLOAT_EQ(restored.x, original.x);
    EXPECT_FLOAT_EQ(restored.y, original.y);
}

TEST_F(JsonTypesTest, Vector3RoundTrip) {
    Vector3<float, Space::World> original(1.0f, 2.0f, 3.0f);
    nlohmann::json j = original;
    auto restored = j.get<Vector3<float, Space::World>>();
    EXPECT_FLOAT_EQ(restored.x, 1.0f);
    EXPECT_FLOAT_EQ(restored.y, 2.0f);
    EXPECT_FLOAT_EQ(restored.z, 3.0f);
}

TEST_F(JsonTypesTest, Vector4RoundTrip) {
    Vector4<float, Space::World> original(1.0f, 2.0f, 3.0f, 4.0f);
    nlohmann::json j = original;
    auto restored = j.get<Vector4<float, Space::World>>();
    EXPECT_FLOAT_EQ(restored.x, 1.0f);
    EXPECT_FLOAT_EQ(restored.y, 2.0f);
    EXPECT_FLOAT_EQ(restored.z, 3.0f);
    EXPECT_FLOAT_EQ(restored.w, 4.0f);
}

TEST_F(JsonTypesTest, QuaternionRoundTrip) {
    Quaternion<float> original(0.1f, 0.2f, 0.3f, 0.9f);
    nlohmann::json j = original;
    auto restored = j.get<Quaternion<float>>();
    EXPECT_FLOAT_EQ(restored.x, 0.1f);
    EXPECT_FLOAT_EQ(restored.y, 0.2f);
    EXPECT_FLOAT_EQ(restored.z, 0.3f);
    EXPECT_FLOAT_EQ(restored.w, 0.9f);
}

TEST_F(JsonTypesTest, Vector3JsonStructure) {
    Vector3<double, Space::Local> v(10.0, 20.0, 30.0);
    nlohmann::json j = v;
    EXPECT_TRUE(j.contains("x"));
    EXPECT_TRUE(j.contains("y"));
    EXPECT_TRUE(j.contains("z"));
    EXPECT_EQ(j["x"].get<double>(), 10.0);
}

TEST_F(JsonTypesTest, FromJsonMissingField) {
    nlohmann::json j = {{"x", 1.0f}, {"y", 2.0f}};
    Vector3<float, Space::World> v;
    EXPECT_THROW(j.get_to(v), nlohmann::json::out_of_range);
}

} // namespace fabric

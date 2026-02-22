#pragma once

#include <nlohmann/json.hpp>
#include "fabric/core/Spatial.hh"

// ADL-visible to_json/from_json for core Fabric spatial types.
// Enables: nlohmann::json j = myVec3; auto v = j.get<Vector3<float>>();

namespace fabric {

// --- Vector2 ---

template <typename T, typename SpaceTag>
void to_json(nlohmann::json& j, const Vector2<T, SpaceTag>& v) {
  j = nlohmann::json{{"x", v.x}, {"y", v.y}};
}

template <typename T, typename SpaceTag>
void from_json(const nlohmann::json& j, Vector2<T, SpaceTag>& v) {
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
}

// --- Vector3 ---

template <typename T, typename SpaceTag>
void to_json(nlohmann::json& j, const Vector3<T, SpaceTag>& v) {
  j = nlohmann::json{{"x", v.x}, {"y", v.y}, {"z", v.z}};
}

template <typename T, typename SpaceTag>
void from_json(const nlohmann::json& j, Vector3<T, SpaceTag>& v) {
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
  j.at("z").get_to(v.z);
}

// --- Vector4 ---

template <typename T, typename SpaceTag>
void to_json(nlohmann::json& j, const Vector4<T, SpaceTag>& v) {
  j = nlohmann::json{{"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w}};
}

template <typename T, typename SpaceTag>
void from_json(const nlohmann::json& j, Vector4<T, SpaceTag>& v) {
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
  j.at("z").get_to(v.z);
  j.at("w").get_to(v.w);
}

// --- Quaternion ---

template <typename T>
void to_json(nlohmann::json& j, const Quaternion<T>& q) {
  j = nlohmann::json{{"x", q.x}, {"y", q.y}, {"z", q.z}, {"w", q.w}};
}

template <typename T>
void from_json(const nlohmann::json& j, Quaternion<T>& q) {
  j.at("x").get_to(q.x);
  j.at("y").get_to(q.y);
  j.at("z").get_to(q.z);
  j.at("w").get_to(q.w);
}

} // namespace fabric

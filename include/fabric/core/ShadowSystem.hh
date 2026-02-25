#pragma once

#include "fabric/core/Camera.hh"
#include "fabric/core/Spatial.hh"
#include <array>
#include <cstdint>

namespace fabric {

enum class ShadowQualityPreset : uint8_t {
    Low,
    Medium,
    High,
    Ultra
};

struct ShadowConfig {
    int cascadeCount = 3;
    std::array<int, 4> cascadeResolution = {2048, 2048, 1024, 512};
    float cascadeSplitLambda = 0.75f;
    float maxShadowDistance = 200.0f;
    float shadowBias = 0.005f;
    int pcfSamples = 4;
    bool enabled = true;
};

ShadowConfig presetConfig(ShadowQualityPreset preset);

struct CascadeData {
    std::array<float, 16> lightViewProj = {};
    float splitDistance = 0.0f;
};

// Cascaded shadow map system. Computes cascade splits and light-space matrices
// for depth-only shadow passes.
//
// bgfx view IDs 240-243 are reserved for shadow cascade rendering passes.
class ShadowSystem {
  public:
    static constexpr uint8_t kShadowViewBase = 240;
    static constexpr int kMaxCascades = 4;

    explicit ShadowSystem(ShadowConfig config = {});

    void setConfig(const ShadowConfig& config);
    const ShadowConfig& config() const;

    // Compute cascade splits and light-space matrices for the current frame.
    void update(const Camera& camera, const Vector3<float, Space::World>& lightDir);

    CascadeData getCascadeData(int cascadeIndex) const;

    // Cascade split distances in view space (array of cascadeCount + 1 values).
    const std::array<float, 5>& splitDistances() const;

  private:
    void computeSplits(float nearPlane, float farPlane);
    void computeLightMatrix(int cascade, const Camera& camera, const Vector3<float, Space::World>& lightDir);

    ShadowConfig config_;
    std::array<CascadeData, 4> cascades_ = {};
    std::array<float, 5> splits_ = {};
};

} // namespace fabric

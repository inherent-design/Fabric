#include "fabric/core/ShadowSystem.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm>
#include <bx/math.h>
#include <cmath>
#include <cstring>

namespace fabric {

ShadowConfig presetConfig(ShadowQualityPreset preset) {
    ShadowConfig cfg;
    switch (preset) {
        case ShadowQualityPreset::Low:
            cfg.cascadeCount = 1;
            cfg.cascadeResolution = {1024, 0, 0, 0};
            cfg.pcfSamples = 0;
            break;
        case ShadowQualityPreset::Medium:
            cfg.cascadeCount = 2;
            cfg.cascadeResolution = {1024, 512, 0, 0};
            cfg.pcfSamples = 4;
            break;
        case ShadowQualityPreset::High:
            cfg.cascadeCount = 3;
            cfg.cascadeResolution = {2048, 2048, 1024, 0};
            cfg.pcfSamples = 4;
            break;
        case ShadowQualityPreset::Ultra:
            cfg.cascadeCount = 3;
            cfg.cascadeResolution = {4096, 2048, 2048, 0};
            cfg.pcfSamples = 9;
            break;
    }
    return cfg;
}

ShadowSystem::ShadowSystem(const ShadowConfig& config) : config_(config) {}

void ShadowSystem::setConfig(const ShadowConfig& config) {
    config_ = config;
}

const ShadowConfig& ShadowSystem::config() const {
    return config_;
}

void ShadowSystem::computeSplits(float nearPlane, float farPlane) {
    FABRIC_ZONE_SCOPED;
    float range = std::min(farPlane, config_.maxShadowDistance);
    float ratio = range / nearPlane;
    float lambda = config_.cascadeSplitLambda;
    int n = config_.cascadeCount;

    splits_[0] = nearPlane;
    for (int i = 1; i <= n; ++i) {
        float p = static_cast<float>(i) / static_cast<float>(n);
        float logSplit = nearPlane * std::pow(ratio, p);
        float linearSplit = nearPlane + (range - nearPlane) * p;
        splits_[i] = lambda * logSplit + (1.0f - lambda) * linearSplit;
    }
}

void ShadowSystem::computeLightMatrix(int cascade, const Camera& camera, const Vector3<float, Space::World>& lightDir) {
    FABRIC_ZONE_SCOPED;

    float invView[16];
    float viewMtx[16];
    std::memcpy(viewMtx, camera.viewMatrix(), sizeof(viewMtx));
    bx::mtxInverse(invView, viewMtx);

    float nearDist = splits_[cascade];
    float farDist = splits_[cascade + 1];

    float fovRad = camera.fovY() * (3.14159265358979f / 180.0f);
    float tanHalfFov = std::tan(fovRad * 0.5f);
    float ar = camera.aspectRatio();

    float nearH = tanHalfFov * nearDist;
    float nearW = nearH * ar;
    float farH = tanHalfFov * farDist;
    float farW = farH * ar;

    // Frustum corners in view space (near face, then far face)
    float cornersRaw[8][3] = {
        {-nearW, -nearH, nearDist}, {nearW, -nearH, nearDist}, {nearW, nearH, nearDist}, {-nearW, nearH, nearDist},
        {-farW, -farH, farDist},    {farW, -farH, farDist},    {farW, farH, farDist},    {-farW, farH, farDist},
    };

    // Transform to world space
    float worldCornersRaw[8][3];
    for (int i = 0; i < 8; ++i) {
        float in[4] = {cornersRaw[i][0], cornersRaw[i][1], cornersRaw[i][2], 1.0f};
        float out[4];
        bx::vec4MulMtx(out, in, invView);
        worldCornersRaw[i][0] = out[0];
        worldCornersRaw[i][1] = out[1];
        worldCornersRaw[i][2] = out[2];
    }

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    for (int i = 0; i < 8; ++i) {
        cx += worldCornersRaw[i][0];
        cy += worldCornersRaw[i][1];
        cz += worldCornersRaw[i][2];
    }
    cx /= 8.0f;
    cy /= 8.0f;
    cz /= 8.0f;

    float dirLen = std::sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
    float ldx = 0.0f, ldy = -1.0f, ldz = 0.0f;
    if (dirLen > 0.0001f) {
        ldx = lightDir.x / dirLen;
        ldy = lightDir.y / dirLen;
        ldz = lightDir.z / dirLen;
    }

    float radius = 0.0f;
    for (int i = 0; i < 8; ++i) {
        float dx = worldCornersRaw[i][0] - cx;
        float dy = worldCornersRaw[i][1] - cy;
        float dz = worldCornersRaw[i][2] - cz;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        radius = std::max(radius, dist);
    }

    bx::Vec3 eye(cx - ldx * radius, cy - ldy * radius, cz - ldz * radius);
    bx::Vec3 at(cx, cy, cz);

    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
    float dotUp = std::abs(ldx * upX + ldy * upY + ldz * upZ);
    if (dotUp > 0.99f) {
        upX = 1.0f;
        upY = 0.0f;
    }
    bx::Vec3 up(upX, upY, upZ);

    float lightView[16];
    bx::mtxLookAt(lightView, eye, at, up);

    float lightProj[16];
    bx::mtxOrtho(lightProj, -radius, radius, -radius, radius, 0.0f, 2.0f * radius, 0.0f, false);

    bx::mtxMul(cascades_[cascade].lightViewProj.data(), lightView, lightProj);
    cascades_[cascade].splitDistance = farDist;
}

void ShadowSystem::update(const Camera& camera, const Vector3<float, Space::World>& lightDir) {
    FABRIC_ZONE_SCOPED;

    if (!config_.enabled) {
        return;
    }

    computeSplits(camera.nearPlane(), camera.farPlane());

    int count = std::min(config_.cascadeCount, kMaxCascades);
    for (int i = 0; i < count; ++i) {
        computeLightMatrix(i, camera, lightDir);
    }
}

CascadeData ShadowSystem::getCascadeData(int cascadeIndex) const {
    if (cascadeIndex < 0 || cascadeIndex >= config_.cascadeCount) {
        throwError("ShadowSystem::getCascadeData: cascade index out of range");
    }
    return cascades_[static_cast<size_t>(cascadeIndex)];
}

const std::array<float, 5>& ShadowSystem::splitDistances() const {
    return splits_;
}

} // namespace fabric

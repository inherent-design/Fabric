#include "recurse/systems/ShadowRenderSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/log/Log.hh"
#include "fabric/render/Camera.hh"
#include "fabric/utils/Profiler.hh"

#include <cmath>

namespace recurse::systems {

void ShadowRenderSystem::doShutdown() {
    FABRIC_ZONE_SCOPED_N("ShadowRenderSystem::doShutdown");

    if (shadowSystem_) {
        FABRIC_LOG_DEBUG("ShadowRenderSystem: releasing ShadowSystem GPU resources");
        shadowSystem_.reset();
    }
    FABRIC_LOG_INFO("ShadowRenderSystem shut down");
}

void ShadowRenderSystem::doInit(fabric::AppContext& /*ctx*/) {
    FABRIC_ZONE_SCOPED_N("ShadowRenderSystem::doInit");

    shadowSystem_ =
        std::make_unique<recurse::ShadowSystem>(recurse::presetConfig(recurse::ShadowQualityPreset::Medium));

    lightDir_ = fabric::Vec3f(0.5f, 0.8f, 0.3f);
    float len = std::sqrt(lightDir_.x * lightDir_.x + lightDir_.y * lightDir_.y + lightDir_.z * lightDir_.z);
    lightDir_ = fabric::Vec3f(lightDir_.x / len, lightDir_.y / len, lightDir_.z / len);

    FABRIC_LOG_INFO("ShadowRenderSystem initialized");
}

void ShadowRenderSystem::render(fabric::AppContext& ctx) {
    FABRIC_ZONE_SCOPED_N("ShadowRenderSystem::render");

    shadowSystem_->update(*ctx.camera,
                          fabric::Vector3<float, fabric::Space::World>(lightDir_.x, lightDir_.y, lightDir_.z));
}

void ShadowRenderSystem::configureDependencies() {
    // No dependencies; shadow computation is independent
}

} // namespace recurse::systems

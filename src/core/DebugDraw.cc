#include "fabric/core/DebugDraw.hh"
#include "fabric/core/Log.hh"

#include <bgfx/bgfx.h>

// bgfx debugdraw from examples/common (compiled into FabricLib via CMake)
#include "debugdraw.h"

namespace fabric {

DebugDraw::~DebugDraw() {
    if (initialized_) {
        shutdown();
    }
}

void DebugDraw::init() {
    if (initialized_) {
        return;
    }

    ddInit();
    encoder_ = new DebugDrawEncoder();
    initialized_ = true;
    FABRIC_LOG_INFO("DebugDraw initialized");
}

void DebugDraw::shutdown() {
    if (!initialized_) {
        return;
    }

    delete static_cast<DebugDrawEncoder*>(encoder_);
    encoder_ = nullptr;

    ddShutdown();
    initialized_ = false;
    flags_ = DebugDrawFlags::None;
    FABRIC_LOG_INFO("DebugDraw shutdown");
}

bool DebugDraw::isInitialized() const {
    return initialized_;
}

void DebugDraw::toggleFlag(DebugDrawFlags flag) {
    flags_ = static_cast<DebugDrawFlags>(static_cast<uint32_t>(flags_) ^ static_cast<uint32_t>(flag));
}

void DebugDraw::setFlag(DebugDrawFlags flag, bool enabled) {
    if (enabled) {
        flags_ |= flag;
    } else {
        flags_ &= ~flag;
    }
}

bool DebugDraw::hasFlag(DebugDrawFlags flag) const {
    return (flags_ & flag) != DebugDrawFlags::None;
}

DebugDrawFlags DebugDraw::flags() const {
    return flags_;
}

void DebugDraw::toggleWireframe() {
    toggleFlag(DebugDrawFlags::Wireframe);
}

bool DebugDraw::isWireframeEnabled() const {
    return hasFlag(DebugDrawFlags::Wireframe);
}

void DebugDraw::applyDebugFlags() {
    if (!initialized_) {
        return;
    }

    uint32_t bgfxDebug = BGFX_DEBUG_NONE;
    if (hasFlag(DebugDrawFlags::Wireframe)) {
        bgfxDebug |= BGFX_DEBUG_WIREFRAME;
    }
    bgfx::setDebug(bgfxDebug);
}

void DebugDraw::begin(uint16_t viewId) {
    if (!initialized_ || !encoder_) {
        return;
    }
    static_cast<DebugDrawEncoder*>(encoder_)->begin(viewId);
}

void DebugDraw::end() {
    if (!initialized_ || !encoder_) {
        return;
    }
    static_cast<DebugDrawEncoder*>(encoder_)->end();
}

void DebugDraw::setColor(uint32_t abgr) {
    if (!initialized_ || !encoder_) {
        return;
    }
    static_cast<DebugDrawEncoder*>(encoder_)->setColor(abgr);
}

void DebugDraw::setWireframe(bool enabled) {
    if (!initialized_ || !encoder_) {
        return;
    }
    static_cast<DebugDrawEncoder*>(encoder_)->setWireframe(enabled);
}

void DebugDraw::drawWireBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    if (!initialized_ || !encoder_) {
        return;
    }
    auto* enc = static_cast<DebugDrawEncoder*>(encoder_);
    // Bottom face
    enc->moveTo(minX, minY, minZ);
    enc->lineTo(maxX, minY, minZ);
    enc->moveTo(maxX, minY, minZ);
    enc->lineTo(maxX, minY, maxZ);
    enc->moveTo(maxX, minY, maxZ);
    enc->lineTo(minX, minY, maxZ);
    enc->moveTo(minX, minY, maxZ);
    enc->lineTo(minX, minY, minZ);
    // Top face
    enc->moveTo(minX, maxY, minZ);
    enc->lineTo(maxX, maxY, minZ);
    enc->moveTo(maxX, maxY, minZ);
    enc->lineTo(maxX, maxY, maxZ);
    enc->moveTo(maxX, maxY, maxZ);
    enc->lineTo(minX, maxY, maxZ);
    enc->moveTo(minX, maxY, maxZ);
    enc->lineTo(minX, maxY, minZ);
    // Vertical edges
    enc->moveTo(minX, minY, minZ);
    enc->lineTo(minX, maxY, minZ);
    enc->moveTo(maxX, minY, minZ);
    enc->lineTo(maxX, maxY, minZ);
    enc->moveTo(maxX, minY, maxZ);
    enc->lineTo(maxX, maxY, maxZ);
    enc->moveTo(minX, minY, maxZ);
    enc->lineTo(minX, maxY, maxZ);
}

} // namespace fabric

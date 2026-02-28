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

} // namespace fabric

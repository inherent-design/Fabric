#pragma once

#include "fabric/core/Spatial.hh"
#include "fabric/ui/RmlPanel.hh"
#include <RmlUi/Core/Types.h>
#include <string>

namespace recurse {

struct DebugData {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    int entityCount = 0;
    int visibleChunks = 0;
    int totalChunks = 0;
    int triangleCount = 0;
    fabric::Vector3<float, fabric::Space::World> cameraPosition;
    int currentRadius = 0;
    std::string currentState = "None";

    // Perf overlay (EF-18)
    int drawCallCount = 0;
    float gpuTimeMs = 0.0f;
    float memoryUsageMB = 0.0f;
    int physicsBodyCount = 0;
    int audioVoiceCount = 0;
    int chunkMeshQueueSize = 0;

    std::string autosaveState = "Idle";
    std::string autosaveNextSave = "-";
    int autosaveDirtyChunks = 0;
    int autosaveSavingChunks = 0;
    int autosaveQueuedChunks = 0;
};

class DebugHUD : public fabric::RmlPanel {
  public:
    DebugHUD() = default;
    ~DebugHUD() override = default;

    void init(Rml::Context* context);
    void update(const DebugData& data);

  private:
    float fps_ = 0.0f;
    float frameTimeMs_ = 0.0f;
    int entityCount_ = 0;
    int visibleChunks_ = 0;
    int totalChunks_ = 0;
    int triangleCount_ = 0;
    float camX_ = 0.0f;
    float camY_ = 0.0f;
    float camZ_ = 0.0f;
    int currentRadius_ = 0;
    Rml::String currentState_;

    // Perf overlay (EF-18)
    int drawCallCount_ = 0;
    float gpuTimeMs_ = 0.0f;
    float memoryUsageMB_ = 0.0f;
    int physicsBodyCount_ = 0;
    int audioVoiceCount_ = 0;
    int chunkMeshQueueSize_ = 0;
    Rml::String autosaveState_;
    Rml::String autosaveNextSave_;
    int autosaveDirtyChunks_ = 0;
    int autosaveSavingChunks_ = 0;
    int autosaveQueuedChunks_ = 0;
};

} // namespace recurse

#pragma once

#include "fabric/core/Spatial.hh"
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>
#include <string>

namespace Rml {
class Context;
class ElementDocument;
} // namespace Rml

namespace fabric {

struct DebugData {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    int entityCount = 0;
    int visibleChunks = 0;
    int totalChunks = 0;
    int triangleCount = 0;
    Vector3<float, Space::World> cameraPosition;
    int currentRadius = 0;
    std::string currentState = "None";

    // Perf overlay (EF-18)
    int drawCallCount = 0;
    float gpuTimeMs = 0.0f;
    float memoryUsageMB = 0.0f;
    int physicsBodyCount = 0;
    int audioVoiceCount = 0;
    int chunkMeshQueueSize = 0;
};

// Debug overlay using RmlUI data binding. Displays engine metrics in a
// semi-transparent panel. Toggle visibility with F3 or via toggle().
class DebugHUD {
  public:
    DebugHUD() = default;
    ~DebugHUD() = default;

    void init(Rml::Context* context);
    void update(const DebugData& data);
    void toggle();
    void shutdown();
    bool isVisible() const;

  private:
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    bool visible_ = false;
    bool initialized_ = false;

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

    Rml::DataModelHandle modelHandle_;
};

} // namespace fabric

#pragma once

#include <flecs.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>
#include <string>
#include <vector>

namespace Rml {
class Context;
class ElementDocument;
} // namespace Rml

namespace fabric {

class BehaviorAI;

struct BTNodeInfo {
    std::string name;
    std::string status; // "SUCCESS", "FAILURE", "RUNNING", "IDLE"
    int depth = 0;      // indentation level
};

class BTDebugPanel {
  public:
    BTDebugPanel() = default;
    ~BTDebugPanel() = default;

    void init(Rml::Context* context);
    void update(BehaviorAI& ai, flecs::entity selectedNpc);
    void toggle();
    void shutdown();
    bool isVisible() const;

    // Cycle through NPCs with BehaviorTreeComponent
    void selectNextNPC(BehaviorAI& ai, flecs::world& world);

    // Current selected NPC entity
    flecs::entity selectedNpc() const;

  private:
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    bool visible_ = false;
    bool initialized_ = false;

    flecs::entity selectedNpc_;
    Rml::String selectedNpcName_;
    Rml::String nodeListText_; // Preformatted text for display

    Rml::DataModelHandle modelHandle_;
};

} // namespace fabric

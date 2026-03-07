#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
} // namespace Rml

namespace fabric {

// Base class for RmlUI panels with data model bindings.
// Provides shared lifecycle (init guard, shutdown cleanup, toggle, visibility).
// Subclasses implement setupDataModel() for their specific bindings and
// handle their own update() / setMode() logic.
class RmlPanel {
  public:
    virtual ~RmlPanel() = default;

    void toggle();
    void shutdown();
    bool isVisible() const;

  protected:
    // Called by subclass init() after setting up the data model.
    // Loads the document from rmlPath, starts hidden.
    bool initBase(Rml::Context* context, const char* modelName, const char* rmlPath);

    // Shared shutdown hook for subclasses that need pre-cleanup.
    // Called before base teardown. Default is no-op.
    virtual void onShutdown() {}

    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    bool visible_ = false;
    bool initialized_ = false;
    Rml::DataModelHandle modelHandle_;

  private:
    Rml::String modelName_;
};

} // namespace fabric

#pragma once

#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Temporal.hh"

namespace fabric {

class ResourceHub; // forward declare to avoid heavy include

struct AppContext {
    World& world;
    Timeline& timeline;
    EventDispatcher& dispatcher;
    ResourceHub& resourceHub;
};

} // namespace fabric

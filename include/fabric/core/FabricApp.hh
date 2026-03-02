#pragma once

#include "fabric/core/FabricAppDesc.hh"

namespace fabric {

/// Static entry point for Fabric applications. Owns the entire lifecycle.
/// All state is local to run(); no member variables, no singleton.
class FabricApp {
  public:
    /// Runs the 9-phase application lifecycle. Returns exit code (0 = success).
    static int run(int argc, char** argv, FabricAppDesc desc);

    FabricApp() = delete;
};

} // namespace fabric

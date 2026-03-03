#pragma once

// Facade header for backward compatibility.
// Includes the full CoordinatedGraph implementation split across:
//   CoordinatedGraphTypes.hh   -- exception types
//   CoordinatedGraphCore.hh    -- graph template + core operations
//   CoordinatedGraphLocking.hh -- resource locking (included by Core)

#include "fabric/utils/CoordinatedGraphCore.hh"
#include "fabric/utils/CoordinatedGraphTypes.hh"

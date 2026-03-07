#pragma once

#include "fabric/utils/ErrorHandling.hh"
#include <string>

namespace fabric {

// KNOWN ISSUES (documented during 2026-02-21 audit):
// - [FIXED] getData() returns unprotected reference -> use getDataNoLock() with held lock
// - [FIXED] processDependencyOrder recursive mutex acquisition -> removed redundant lock
// - [FIXED] currentStructuralIntent_ data race -> replaced with std::atomic<int>
// - [FIXED] ABBA deadlock -> wouldCauseDeadlock restructured to match lock ordering

/**
 * @brief Exception thrown when a cycle is detected in the graph
 */
class CycleDetectedException : public FabricException {
  public:
    explicit CycleDetectedException(const std::string& message) : FabricException(message) {}
};

/**
 * @brief Exception thrown when a lock cannot be acquired
 */
class LockAcquisitionException : public FabricException {
  public:
    explicit LockAcquisitionException(const std::string& message) : FabricException(message) {}
};

/**
 * @brief Exception thrown when a deadlock is detected in the lock graph
 */
class DeadlockDetectedException : public FabricException {
  public:
    explicit DeadlockDetectedException(const std::string& message) : FabricException(message) {}
};

/**
 * @brief Exception thrown when a lock acquisition times out
 */
class LockTimeoutException : public FabricException {
  public:
    explicit LockTimeoutException(const std::string& message) : FabricException(message) {}
};

} // namespace fabric

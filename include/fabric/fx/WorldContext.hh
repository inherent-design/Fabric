#pragma once

#include "fabric/core/CompilerHints.hh"
#include "fabric/fx/WorldOps.hh"

namespace fabric::fx {

/// Centralized executor routing world interactions through explicit operations.
///
/// Template parameter Session allows different concrete backends, such as the
/// production WorldSession, test doubles, or replay sessions. No virtual
/// dispatch is required; operations resolve via concepts and template
/// deduction.
///
/// Sync reads compile to direct pointer chases. Async mutations are queued and
/// batched by the session. The migration is intentionally incremental:
/// resolve() and submit() are the main boundary today, while new mesh-facing
/// semantic or query work should prefer explicit ops over direct session access.
template <typename Session> class WorldContext {
  public:
    explicit WorldContext(Session& session) : session_(session) {}

    WorldContext(const WorldContext&) = delete;
    WorldContext& operator=(const WorldContext&) = delete;

    /// Resolve a synchronous read operation inline.
    ///
    /// For infallible ops where Errors is Never, the result collapses to the
    /// operation's Returns type.
    template <SyncReadOp Op>
        requires Resolves<Session, Op>
    FABRIC_ALWAYS_INLINE auto resolve(const Op& op) {
        return session_.resolve(op);
    }

    /// Submit an asynchronous mutation operation to the session backend.
    template <AsyncMutationOp Op>
        requires Accepts<Session, Op>
    auto submit(Op op) {
        return session_.submit(std::move(op));
    }

    /// Access the concrete session while a surface is still being migrated.
    ///
    /// Exists as an incremental escape hatch only. Prefer adding explicit ops
    /// or query surfaces for new behavior.
    Session& session() { return session_; }
    const Session& session() const { return session_; }

  private:
    Session& session_;
};

} // namespace fabric::fx

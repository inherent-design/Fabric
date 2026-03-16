#pragma once

#include "fabric/fx/WorldOps.hh"

namespace fabric::fx {

/// Centralized executor routing all world state interactions.
///
/// Template parameter Session allows different backends (production
/// WorldSession, test mocks, replay sessions). No virtual dispatch;
/// operations resolved via concepts and template deduction.
///
/// Sync reads compile to direct pointer chases (zero overhead via
/// always_inline). Async mutations are queued and batched by the session.
template <typename Session> class WorldContext {
  public:
    explicit WorldContext(Session& session) : session_(session) {}

    WorldContext(const WorldContext&) = delete;
    WorldContext& operator=(const WorldContext&) = delete;

    /// Resolve a synchronous read operation. Returns immediately.
    /// For infallible ops (Errors = Never), the result collapses to just Returns.
    template <SyncReadOp Op>
        requires Resolves<Session, Op>
    [[gnu::always_inline]] auto resolve(const Op& op) -> decltype(session_.resolve(op)) {
        return session_.resolve(op);
    }

    /// Submit an asynchronous mutation operation.
    template <AsyncMutationOp Op>
        requires Accepts<Session, Op>
    auto submit(Op op) -> decltype(session_.submit(std::move(op))) {
        return session_.submit(std::move(op));
    }

    /// Direct session access for operations not yet migrated.
    /// Exists during incremental migration only.
    Session& session() { return session_; }
    const Session& session() const { return session_; }

  private:
    Session& session_;
};

} // namespace fabric::fx

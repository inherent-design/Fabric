#pragma once

#include "fabric/fx/Never.hh"
#include "fabric/fx/OneOf.hh"
#include "fabric/fx/Result.hh"

#include <concepts>
#include <type_traits>

namespace fabric::fx {

/// Synchronous read operation resolved inline with zero overhead.
template <typename Op>
concept SyncReadOp = requires {
    typename Op::Returns;
    typename Op::Errors;
    { Op::K_IS_SYNC } -> std::convertible_to<bool>;
    requires Op::K_IS_SYNC == true;
};

/// Asynchronous mutation operation queued for deferred execution.
template <typename Op>
concept AsyncMutationOp = requires {
    typename Op::Returns;
    typename Op::Errors;
    { Op::K_IS_SYNC } -> std::convertible_to<bool>;
    requires Op::K_IS_SYNC == false;
};

/// Any well-formed operation (sync or async).
template <typename Op>
concept WorldOp = SyncReadOp<Op> || AsyncMutationOp<Op>;

/// Session type can resolve a specific sync read operation.
template <typename Session, typename Op>
concept Resolves = requires(Session& s, const Op& op) {
    { s.resolve(op) };
};

/// Session type can accept a specific async mutation operation.
template <typename Session, typename Op>
concept Accepts = requires(Session& s, Op op) {
    { s.submit(std::move(op)) };
};

/// Compute the Result type for an operation from its Returns and Errors.
template <typename Op> struct OpResult;

template <typename Op>
    requires requires {
        typename Op::Returns;
        typename Op::Errors;
    }
struct OpResult<Op> {
  private:
    template <typename Returns, typename ErrorList> struct Impl;

    template <typename Returns, typename... Es> struct Impl<Returns, TypeList<Es...>> {
        using type = Result<Returns, Es...>;
    };

  public:
    using type = typename Impl<typename Op::Returns, typename Op::Errors>::type;
};

template <typename Op> using OpResult_t = typename OpResult<Op>::type;

} // namespace fabric::fx

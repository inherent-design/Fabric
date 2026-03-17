#pragma once

#include <concepts>
#include <optional>
#include <utility>
#include <variant>

#include "fabric/core/CompilerHints.hh"
#include "fabric/fx/OneOf.hh"

namespace fabric::fx {

template <typename A, typename... Es> class Result;

namespace detail {

template <typename A, typename List> struct ResultFromList;

template <typename A, typename... Es> struct ResultFromList<A, TypeList<Es...>> {
    using type = Result<A, Es...>;
};

template <typename T> struct ErrorsOf;

template <typename A, typename... Es> struct ErrorsOf<Result<A, Es...>> {
    using type = TypeList<Es...>;
};

template <typename T> struct SuccessOf;

template <typename A, typename... Es> struct SuccessOf<Result<A, Es...>> {
    using type = A;
};

// Only called when isFailure(); A branch is unreachable.
template <typename A, typename... Es> std::variant<Es...> extractError(const std::variant<A, Es...>& data) {
    return std::visit(overloaded{[](const A&) -> std::variant<Es...> { FABRIC_UNREACHABLE; },
                                 [](const auto& err) -> std::variant<Es...> { return err; }},
                      data);
}

template <typename A, typename... Es> std::variant<Es...> extractError(std::variant<A, Es...>& data) {
    return std::visit(overloaded{[](A&) -> std::variant<Es...> { FABRIC_UNREACHABLE; },
                                 [](auto& err) -> std::variant<Es...> { return std::move(err); }},
                      data);
}

} // namespace detail

/// Composable result type with typed error channels.
///
/// Stores std::variant<A, Es...> where index 0 is the success value.
template <typename A, typename... Es> class Result {
    static_assert(sizeof...(Es) > 0, "Result requires at least one error type; use Result<A, Never> for infallible");
    static_assert(!contains_v<A, Es...>, "Success type must not appear in the error type list");

    std::variant<A, Es...> data_;

    explicit Result(std::variant<A, Es...> data) : data_(std::move(data)) {}

    template <typename, typename...> friend class Result;

  public:
    using SuccessType = A;

    static Result success(const A& value) { return Result(std::variant<A, Es...>(std::in_place_index<0>, value)); }

    static Result success(A&& value) {
        return Result(std::variant<A, Es...>(std::in_place_index<0>, std::move(value)));
    }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    static Result failure(E error) {
        return Result(std::variant<A, Es...>(std::move(error)));
    }

    bool isSuccess() const { return data_.index() == 0; }
    bool isFailure() const { return data_.index() != 0; }
    explicit operator bool() const { return isSuccess(); }

    A& value() & { return std::get<0>(data_); }
    const A& value() const& { return std::get<0>(data_); }
    A&& value() && { return std::get<0>(std::move(data_)); }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    E& error() & {
        return std::get<E>(data_);
    }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    const E& error() const& {
        return std::get<E>(data_);
    }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    E&& error() && {
        return std::get<E>(std::move(data_));
    }

    /// F: A -> B. Errors pass through unchanged.
    template <typename F>
        requires std::invocable<F, A&>
    auto map(F&& f) -> Result<std::invoke_result_t<F, A&>, Es...> {
        using B = std::invoke_result_t<F, A&>;
        using Ret = Result<B, Es...>;
        if (isSuccess())
            return Ret::success(std::forward<F>(f)(value()));
        auto errVar = detail::extractError<A, Es...>(data_);
        return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, errVar);
    }

    template <typename F>
        requires std::invocable<F, const A&>
    auto map(F&& f) const -> Result<std::invoke_result_t<F, const A&>, Es...> {
        using B = std::invoke_result_t<F, const A&>;
        using Ret = Result<B, Es...>;
        if (isSuccess())
            return Ret::success(std::forward<F>(f)(value()));
        auto errVar = detail::extractError<A, Es...>(data_);
        return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, errVar);
    }

    /// F: A -> Result<B, NewEs...>. Short-circuits on first error.
    template <typename F>
        requires std::invocable<F, A&>
    auto flatMap(F&& f) {
        using InnerResult = std::invoke_result_t<F, A&>;
        using B = typename detail::SuccessOf<InnerResult>::type;
        using NewErrors = typename detail::ErrorsOf<InnerResult>::type;
        using Merged = typename MergeErrors<TypeList<Es...>, NewErrors>::type;
        using Ret = typename detail::ResultFromList<B, Merged>::type;

        if (isSuccess()) {
            auto inner = std::forward<F>(f)(value());
            if (inner.isSuccess())
                return Ret::success(std::move(inner).value());
            auto innerErr = detail::extractError(inner.data_);
            return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, innerErr);
        }
        auto errVar = detail::extractError<A, Es...>(data_);
        return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, errVar);
    }

    template <typename F>
        requires std::invocable<F, const A&>
    auto flatMap(F&& f) const {
        using InnerResult = std::invoke_result_t<F, const A&>;
        using B = typename detail::SuccessOf<InnerResult>::type;
        using NewErrors = typename detail::ErrorsOf<InnerResult>::type;
        using Merged = typename MergeErrors<TypeList<Es...>, NewErrors>::type;
        using Ret = typename detail::ResultFromList<B, Merged>::type;

        if (isSuccess()) {
            auto inner = std::forward<F>(f)(value());
            if (inner.isSuccess())
                return Ret::success(std::move(inner).value());
            auto innerErr = detail::extractError(inner.data_);
            return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, innerErr);
        }
        auto errVar = detail::extractError<A, Es...>(data_);
        return std::visit([](const auto& err) -> Ret { return Ret::failure(err); }, errVar);
    }

    template <typename F> auto andThen(F&& f) { return flatMap(std::forward<F>(f)); }

    template <typename F> auto andThen(F&& f) const { return flatMap(std::forward<F>(f)); }

    /// F: OneOf<Es...> -> NewE. Success passes through unchanged.
    template <typename F>
        requires std::invocable<F, OneOf<Es...>>
    auto mapError(F&& f) -> Result<A, std::invoke_result_t<F, OneOf<Es...>>> {
        using NewE = std::invoke_result_t<F, OneOf<Es...>>;
        if (isSuccess())
            return Result<A, NewE>::success(std::move(value()));
        auto errVar = detail::extractError<A, Es...>(data_);
        auto oneOf = std::visit([](auto& err) -> OneOf<Es...> { return OneOf<Es...>(std::move(err)); }, errVar);
        return Result<A, NewE>::failure(std::forward<F>(f)(std::move(oneOf)));
    }

    template <typename F>
        requires std::invocable<F, OneOf<Es...>>
    auto mapError(F&& f) const -> Result<A, std::invoke_result_t<F, OneOf<Es...>>> {
        using NewE = std::invoke_result_t<F, OneOf<Es...>>;
        if (isSuccess())
            return Result<A, NewE>::success(value());
        auto errVar = detail::extractError<A, Es...>(data_);
        auto oneOf = std::visit([](const auto& err) -> OneOf<Es...> { return OneOf<Es...>(err); }, errVar);
        return Result<A, NewE>::failure(std::forward<F>(f)(std::move(oneOf)));
    }

    template <typename OnSuccess, typename OnFailure>
    auto match(OnSuccess&& onSuccess, OnFailure&& onFailure) const -> std::invoke_result_t<OnSuccess, const A&> {
        using R = std::invoke_result_t<OnSuccess, const A&>;
        if (isSuccess())
            return std::forward<OnSuccess>(onSuccess)(value());
        auto errVar = detail::extractError<A, Es...>(data_);
        return std::visit([&](const auto& err) -> R { return std::forward<OnFailure>(onFailure)(OneOf<Es...>(err)); },
                          errVar);
    }

    template <typename OnSuccess, typename OnFailure>
    auto match(OnSuccess&& onSuccess, OnFailure&& onFailure) -> std::invoke_result_t<OnSuccess, A&> {
        using R = std::invoke_result_t<OnSuccess, A&>;
        if (isSuccess())
            return std::forward<OnSuccess>(onSuccess)(value());
        auto errVar = detail::extractError<A, Es...>(data_);
        return std::visit(
            [&](auto& err) -> R { return std::forward<OnFailure>(onFailure)(OneOf<Es...>(std::move(err))); }, errVar);
    }
};

/// Infallible specialization. Collapses to just A.
template <typename A> class Result<A, Never> {
    A data_;

    explicit Result(A data) : data_(std::move(data)) {}

  public:
    using SuccessType = A;

    static Result success(const A& value) { return Result(value); }
    static Result success(A&& value) { return Result(std::move(value)); }

    constexpr bool isSuccess() const { return true; }
    constexpr bool isFailure() const { return false; }
    explicit constexpr operator bool() const { return true; }

    A& value() & { return data_; }
    const A& value() const& { return data_; }
    A&& value() && { return std::move(data_); }

    template <typename F>
        requires std::invocable<F, A&>
    auto map(F&& f) -> Result<std::invoke_result_t<F, A&>, Never> {
        return Result<std::invoke_result_t<F, A&>, Never>::success(std::forward<F>(f)(data_));
    }

    template <typename F>
        requires std::invocable<F, const A&>
    auto map(F&& f) const -> Result<std::invoke_result_t<F, const A&>, Never> {
        return Result<std::invoke_result_t<F, const A&>, Never>::success(std::forward<F>(f)(data_));
    }

    template <typename F>
        requires std::invocable<F, A&>
    auto flatMap(F&& f) {
        return std::forward<F>(f)(data_);
    }

    template <typename F>
        requires std::invocable<F, const A&>
    auto flatMap(F&& f) const {
        return std::forward<F>(f)(data_);
    }

    template <typename F> auto andThen(F&& f) { return flatMap(std::forward<F>(f)); }

    template <typename F> auto andThen(F&& f) const { return flatMap(std::forward<F>(f)); }

    template <typename OnSuccess, typename OnFailure>
    auto match(OnSuccess&& onSuccess, OnFailure&&) const -> std::invoke_result_t<OnSuccess, const A&> {
        return std::forward<OnSuccess>(onSuccess)(data_);
    }

    template <typename OnSuccess, typename OnFailure>
    auto match(OnSuccess&& onSuccess, OnFailure&&) -> std::invoke_result_t<OnSuccess, A&> {
        return std::forward<OnSuccess>(onSuccess)(data_);
    }
};

/// Void success specialization with typed errors.
template <typename... Es> class Result<void, Es...> {
    static_assert(sizeof...(Es) > 0, "Result<void> requires at least one error type");

    std::optional<std::variant<Es...>> error_;

    Result() = default;
    explicit Result(std::variant<Es...> error) : error_(std::move(error)) {}

    template <typename, typename...> friend class Result;

  public:
    using SuccessType = void;

    static Result success() { return Result(); }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    static Result failure(E error) {
        return Result(std::variant<Es...>(std::move(error)));
    }

    bool isSuccess() const { return !error_.has_value(); }
    bool isFailure() const { return error_.has_value(); }
    explicit operator bool() const { return isSuccess(); }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    E& error() & {
        return std::get<E>(*error_);
    }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    const E& error() const& {
        return std::get<E>(*error_);
    }

    /// F: () -> B.
    template <typename F>
        requires(std::invocable<F> && !std::is_void_v<std::invoke_result_t<F>>)
    auto map(F&& f) -> Result<std::invoke_result_t<F>, Es...> {
        using B = std::invoke_result_t<F>;
        using Ret = Result<B, Es...>;
        if (isSuccess())
            return Ret::success(std::forward<F>(f)());
        return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, *error_);
    }

    template <typename F>
        requires(std::invocable<F> && !std::is_void_v<std::invoke_result_t<F>>)
    auto map(F&& f) const -> Result<std::invoke_result_t<F>, Es...> {
        using B = std::invoke_result_t<F>;
        using Ret = Result<B, Es...>;
        if (isSuccess())
            return Ret::success(std::forward<F>(f)());
        return std::visit([](const auto& err) -> Ret { return Ret::failure(err); }, *error_);
    }

    /// F: () -> Result<B, NewEs...>.
    template <typename F>
        requires std::invocable<F>
    auto flatMap(F&& f) {
        using InnerResult = std::invoke_result_t<F>;
        using B = typename detail::SuccessOf<InnerResult>::type;
        using NewErrors = typename detail::ErrorsOf<InnerResult>::type;
        using Merged = typename MergeErrors<TypeList<Es...>, NewErrors>::type;
        using Ret = typename detail::ResultFromList<B, Merged>::type;

        if (isSuccess()) {
            auto inner = std::forward<F>(f)();
            if constexpr (std::is_void_v<B>) {
                if (inner.isSuccess())
                    return Ret::success();
                return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, *inner.error_);
            } else {
                if (inner.isSuccess())
                    return Ret::success(std::move(inner).value());
                auto innerErr = detail::extractError(inner.data_);
                return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, innerErr);
            }
        }
        return std::visit([](auto& err) -> Ret { return Ret::failure(std::move(err)); }, *error_);
    }

    template <typename F> auto andThen(F&& f) { return flatMap(std::forward<F>(f)); }

    template <typename OnSuccess, typename OnFailure>
    auto match(OnSuccess&& onSuccess, OnFailure&& onFailure) const -> std::invoke_result_t<OnSuccess> {
        using R = std::invoke_result_t<OnSuccess>;
        if (isSuccess())
            return std::forward<OnSuccess>(onSuccess)();
        return std::visit([&](const auto& err) -> R { return std::forward<OnFailure>(onFailure)(OneOf<Es...>(err)); },
                          *error_);
    }
};

/// Void success, infallible specialization.
template <> class Result<void, Never> {
  public:
    using SuccessType = void;

    static Result success() { return Result(); }

    constexpr bool isSuccess() const { return true; }
    constexpr bool isFailure() const { return false; }
    explicit constexpr operator bool() const { return true; }

    template <typename OnSuccess, typename OnFailure>
    auto match(OnSuccess&& onSuccess, OnFailure&&) const -> std::invoke_result_t<OnSuccess> {
        return std::forward<OnSuccess>(onSuccess)();
    }
};

} // namespace fabric::fx

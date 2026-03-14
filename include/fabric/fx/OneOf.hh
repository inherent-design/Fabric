#pragma once

#include <type_traits>
#include <variant>

#include "fabric/fx/Never.hh"

namespace fabric::fx {

template <typename T, typename... Ts> inline constexpr bool contains_v = (std::is_same_v<T, Ts> || ...);

template <typename... Ts> struct TypeList {};

template <typename List, typename T> struct AppendUnique;

template <typename... Ts, typename T> struct AppendUnique<TypeList<Ts...>, T> {
    using type = std::conditional_t<contains_v<T, Ts...>, TypeList<Ts...>, TypeList<Ts..., T>>;
};

template <typename L1, typename L2> struct MergeTypeLists;

template <typename... As> struct MergeTypeLists<TypeList<As...>, TypeList<>> {
    using type = TypeList<As...>;
};

template <typename... As, typename B, typename... Bs> struct MergeTypeLists<TypeList<As...>, TypeList<B, Bs...>> {
    using type = typename MergeTypeLists<typename AppendUnique<TypeList<As...>, B>::type, TypeList<Bs...>>::type;
};

template <typename L1, typename L2> using MergeErrors = MergeTypeLists<L1, L2>;

/// Typed error union over std::variant.
template <typename... Es> class OneOf {
    std::variant<Es...> value_;

  public:
    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    explicit OneOf(E error) : value_(std::move(error)) {}

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    bool is() const {
        return std::holds_alternative<E>(value_);
    }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    const E& as() const {
        return std::get<E>(value_);
    }

    template <typename E>
        requires(std::is_same_v<E, Es> || ...)
    E& as() {
        return std::get<E>(value_);
    }

    const auto& variant() const { return value_; }
    auto& variant() { return value_; }

    template <typename... Wider>
        requires(contains_v<Es, Wider...> && ...)
    OneOf<Wider...> widen() const {
        return std::visit([](const auto& err) -> OneOf<Wider...> { return OneOf<Wider...>(err); }, value_);
    }
};

template <typename... Fs> struct overloaded : Fs... {
    using Fs::operator()...;
};
template <typename... Fs> overloaded(Fs...) -> overloaded<Fs...>;

} // namespace fabric::fx

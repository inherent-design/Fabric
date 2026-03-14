#include "fabric/fx/OneOf.hh"
#include "fabric/fx/Error.hh"
#include "fabric/fx/Never.hh"
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

using namespace fabric::fx;

struct AlphaError {
    std::string detail;
};

struct BetaError {
    int code{0};
};

struct GammaError {
    double value{0.0};
};

TEST(OneOfTest, ConstructFromError) {
    OneOf<IOError, NotFound> err(NotFound{"key", {"missing"}});
    EXPECT_TRUE(err.is<NotFound>());
    EXPECT_FALSE(err.is<IOError>());
}

TEST(OneOfTest, IsChecksAlternative) {
    OneOf<AlphaError, BetaError> err(AlphaError{"oops"});
    EXPECT_TRUE(err.is<AlphaError>());
    EXPECT_FALSE(err.is<BetaError>());

    OneOf<AlphaError, BetaError> err2(BetaError{42});
    EXPECT_FALSE(err2.is<AlphaError>());
    EXPECT_TRUE(err2.is<BetaError>());
}

TEST(OneOfTest, AsReturnsReference) {
    OneOf<AlphaError, BetaError> err(AlphaError{"detail"});
    const auto& alpha = err.as<AlphaError>();
    EXPECT_EQ(alpha.detail, "detail");
}

TEST(OneOfTest, AsMutableReference) {
    OneOf<AlphaError, BetaError> err(BetaError{10});
    err.as<BetaError>().code = 99;
    EXPECT_EQ(err.as<BetaError>().code, 99);
}

TEST(OneOfTest, VariantAccess) {
    OneOf<AlphaError, BetaError> err(AlphaError{"test"});
    const auto& var = err.variant();
    EXPECT_TRUE(std::holds_alternative<AlphaError>(var));
    EXPECT_FALSE(std::holds_alternative<BetaError>(var));
}

TEST(OneOfTest, WidenToSupersetType) {
    OneOf<AlphaError, BetaError> narrow(AlphaError{"narrow"});
    OneOf<AlphaError, BetaError, GammaError> wide = narrow.widen<AlphaError, BetaError, GammaError>();
    EXPECT_TRUE(wide.is<AlphaError>());
    EXPECT_EQ(wide.as<AlphaError>().detail, "narrow");
}

TEST(OneOfTest, WidenPreservesActiveAlternative) {
    OneOf<AlphaError, BetaError> narrow(BetaError{77});
    auto wide = narrow.widen<AlphaError, BetaError, GammaError>();
    EXPECT_FALSE(wide.is<AlphaError>());
    EXPECT_TRUE(wide.is<BetaError>());
    EXPECT_EQ(wide.as<BetaError>().code, 77);
    EXPECT_FALSE(wide.is<GammaError>());
}

TEST(TypeListTest, AppendUniqueAddsNew) {
    using L = TypeList<int, double>;
    using R = AppendUnique<L, float>::type;
    static_assert(std::is_same_v<R, TypeList<int, double, float>>);
}

TEST(TypeListTest, AppendUniqueSkipsDuplicate) {
    using L = TypeList<int, double>;
    using R = AppendUnique<L, int>::type;
    static_assert(std::is_same_v<R, TypeList<int, double>>);
}

TEST(TypeListTest, MergeTypeListsEmpty) {
    using L = TypeList<int, double>;
    using R = MergeTypeLists<L, TypeList<>>::type;
    static_assert(std::is_same_v<R, TypeList<int, double>>);
}

TEST(TypeListTest, MergeTypeListsDeduplicates) {
    using L1 = TypeList<int, double>;
    using L2 = TypeList<double, float>;
    using R = MergeTypeLists<L1, L2>::type;
    static_assert(std::is_same_v<R, TypeList<int, double, float>>);
}

TEST(TypeListTest, MergeTypeListsPreservesOrder) {
    using L1 = TypeList<AlphaError>;
    using L2 = TypeList<BetaError, AlphaError>;
    using R = MergeTypeLists<L1, L2>::type;
    static_assert(std::is_same_v<R, TypeList<AlphaError, BetaError>>);
}

TEST(TypeListTest, ContainsV) {
    static_assert(contains_v<int, int, double, float>);
    static_assert(!contains_v<char, int, double, float>);
    static_assert(contains_v<AlphaError, AlphaError, BetaError>);
    static_assert(!contains_v<GammaError, AlphaError, BetaError>);
}

TEST(NeverTest, IsNotDefaultConstructible) {
    static_assert(!std::is_default_constructible_v<Never>);
}

TEST(NeverTest, IsNotCopyConstructible) {
    static_assert(!std::is_default_constructible_v<Never>);
    static_assert(!std::is_constructible_v<Never>);
}

TEST(NeverTest, IsNotMoveConstructible) {
    static_assert(!std::is_default_constructible_v<Never>);
    static_assert(std::is_trivially_copyable_v<Never>);
}

TEST(OverloadedTest, VisitWithOverloaded) {
    OneOf<AlphaError, BetaError> err(BetaError{42});
    auto result =
        std::visit(overloaded{[](const AlphaError&) -> std::string { return "alpha"; },
                              [](const BetaError& b) -> std::string { return "beta:" + std::to_string(b.code); }},
                   err.variant());
    EXPECT_EQ(result, "beta:42");
}

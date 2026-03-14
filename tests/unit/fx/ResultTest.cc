#include "fabric/fx/Result.hh"
#include "fabric/fx/Error.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace fabric::fx;

// --- ResultTest suite: Result<A, Es...> ---

TEST(ResultTest, SuccessConstruction) {
    auto r = Result<int, IOError>::success(42);
    EXPECT_TRUE(r.isSuccess());
}

TEST(ResultTest, FailureConstruction) {
    auto r = Result<int, IOError>::failure(IOError{"/tmp/x", 2, {"no such file"}});
    EXPECT_TRUE(r.isFailure());
}

TEST(ResultTest, IsSuccessIsFailure) {
    auto ok = Result<int, NotFound>::success(1);
    auto err = Result<int, NotFound>::failure(NotFound{"key", {"missing"}});

    EXPECT_TRUE(ok.isSuccess());
    EXPECT_FALSE(ok.isFailure());
    EXPECT_FALSE(err.isSuccess());
    EXPECT_TRUE(err.isFailure());
}

TEST(ResultTest, OperatorBool) {
    auto ok = Result<int, IOError>::success(7);
    auto err = Result<int, IOError>::failure(IOError{"/f", 1, {"fail"}});

    EXPECT_TRUE(static_cast<bool>(ok));
    EXPECT_FALSE(static_cast<bool>(err));
}

TEST(ResultTest, ValueAccess) {
    auto r = Result<std::string, IOError>::success("hello");
    EXPECT_EQ(r.value(), "hello");
}

TEST(ResultTest, ValueRefQualifiers) {
    auto r = Result<std::string, IOError>::success("test");
    std::string& ref = r.value();
    EXPECT_EQ(ref, "test");

    const auto& cr = r;
    const std::string& cref = cr.value();
    EXPECT_EQ(cref, "test");

    auto r2 = Result<std::string, IOError>::success("move");
    std::string moved = std::move(r2).value();
    EXPECT_EQ(moved, "move");
}

TEST(ResultTest, ErrorAccess) {
    auto r = Result<int, NotFound, IOError>::failure(NotFound{"k", {"missing"}});
    EXPECT_EQ(r.error<NotFound>().key, "k");
}

TEST(ResultTest, MapTransformsSuccess) {
    auto r = Result<int, IOError>::success(10);
    auto mapped = r.map([](int& v) { return v * 2; });
    ASSERT_TRUE(mapped.isSuccess());
    EXPECT_EQ(mapped.value(), 20);
}

TEST(ResultTest, MapPassesThroughError) {
    auto r = Result<int, IOError>::failure(IOError{"/x", 5, {"err"}});
    auto mapped = r.map([](int& v) { return v * 2; });
    ASSERT_TRUE(mapped.isFailure());
    EXPECT_EQ(mapped.error<IOError>().code, 5);
}

TEST(ResultTest, MapConstOverload) {
    const auto r = Result<int, IOError>::success(3);
    auto mapped = r.map([](const int& v) { return v + 1; });
    ASSERT_TRUE(mapped.isSuccess());
    EXPECT_EQ(mapped.value(), 4);
}

TEST(ResultTest, FlatMapChainsSuccess) {
    auto r = Result<int, IOError>::success(5);
    auto chained = r.flatMap([](int& v) { return Result<std::string, IOError>::success(std::to_string(v)); });
    ASSERT_TRUE(chained.isSuccess());
    EXPECT_EQ(chained.value(), "5");
}

TEST(ResultTest, FlatMapShortCircuitsOnError) {
    auto r = Result<int, IOError>::failure(IOError{"/f", 1, {"fail"}});
    bool called = false;
    auto chained = r.flatMap([&](int& v) {
        called = true;
        return Result<int, IOError>::success(v);
    });
    EXPECT_FALSE(called);
    EXPECT_TRUE(chained.isFailure());
}

TEST(ResultTest, FlatMapInnerFailurePropagates) {
    auto r = Result<int, IOError>::success(1);
    auto chained = r.flatMap([](int&) { return Result<int, IOError>::failure(IOError{"/inner", 99, {"inner fail"}}); });
    ASSERT_TRUE(chained.isFailure());
    EXPECT_EQ(chained.error<IOError>().code, 99);
}

TEST(ResultTest, FlatMapMergesErrorTypes) {
    auto r = Result<int, IOError>::success(1);
    auto chained = r.flatMap([](int& v) { return Result<int, NotFound>::success(v + 1); });
    ASSERT_TRUE(chained.isSuccess());
    EXPECT_EQ(chained.value(), 2);

    auto r2 = Result<int, IOError>::failure(IOError{"/x", 1, {"err"}});
    auto chained2 = r2.flatMap([](int& v) { return Result<int, NotFound>::success(v); });
    EXPECT_TRUE(chained2.isFailure());
}

TEST(ResultTest, AndThenAliasForFlatMap) {
    auto r = Result<int, IOError>::success(10);
    auto chained = r.andThen([](int& v) { return Result<int, IOError>::success(v + 5); });
    ASSERT_TRUE(chained.isSuccess());
    EXPECT_EQ(chained.value(), 15);
}

TEST(ResultTest, MapErrorTransformsError) {
    auto r = Result<int, IOError, NotFound>::failure(IOError{"/x", 2, {"err"}});
    auto mapped = r.mapError([](OneOf<IOError, NotFound> e) { return std::string("mapped error"); });
    ASSERT_TRUE(mapped.isFailure());
    EXPECT_EQ(mapped.error<std::string>(), "mapped error");
}

TEST(ResultTest, MapErrorPassesThroughSuccess) {
    auto r = Result<int, IOError>::success(42);
    auto mapped = r.mapError([](OneOf<IOError> e) { return std::string("should not be called"); });
    ASSERT_TRUE(mapped.isSuccess());
    EXPECT_EQ(mapped.value(), 42);
}

TEST(ResultTest, MatchSuccess) {
    auto r = Result<int, IOError>::success(7);
    auto result = r.match([](const int& v) { return v * 3; }, [](OneOf<IOError>) { return -1; });
    EXPECT_EQ(result, 21);
}

TEST(ResultTest, MatchFailure) {
    auto r = Result<int, IOError>::failure(IOError{"/x", 42, {"err"}});
    auto result = r.match([](const int&) { return -1; }, [](OneOf<IOError> e) { return e.as<IOError>().code; });
    EXPECT_EQ(result, 42);
}

TEST(ResultTest, MoveSemantics) {
    auto r = Result<std::unique_ptr<int>, IOError>::success(std::make_unique<int>(99));
    ASSERT_TRUE(r.isSuccess());

    std::unique_ptr<int> extracted = std::move(r).value();
    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(*extracted, 99);

    auto r2 = Result<std::unique_ptr<int>, IOError>::success(std::make_unique<int>(50));
    auto mapped = r2.map([](std::unique_ptr<int>& p) { return *p + 1; });
    ASSERT_TRUE(mapped.isSuccess());
    EXPECT_EQ(mapped.value(), 51);
}

// --- NeverResultTest suite: Result<A, Never> ---

TEST(NeverResultTest, AlwaysSuccess) {
    auto r = Result<int, Never>::success(42);
    EXPECT_TRUE(r.isSuccess());
    EXPECT_FALSE(r.isFailure());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(r.value(), 42);
}

TEST(NeverResultTest, MapPreservesInfallible) {
    auto r = Result<int, Never>::success(5);
    auto mapped = r.map([](int& v) { return v * 2; });
    EXPECT_TRUE(mapped.isSuccess());
    EXPECT_EQ(mapped.value(), 10);
}

TEST(NeverResultTest, FlatMapCanIntroduceErrors) {
    auto r = Result<int, Never>::success(5);
    auto chained = r.flatMap([](int& v) { return Result<std::string, IOError>::success(std::to_string(v)); });
    ASSERT_TRUE(chained.isSuccess());
    EXPECT_EQ(chained.value(), "5");
}

// --- VoidResultTest suite: Result<void, Es...> ---

TEST(VoidResultTest, SuccessConstruction) {
    auto r = Result<void, IOError>::success();
    EXPECT_TRUE(r.isSuccess());
    EXPECT_FALSE(r.isFailure());
}

TEST(VoidResultTest, FailureConstruction) {
    auto r = Result<void, IOError>::failure(IOError{"/f", 1, {"fail"}});
    EXPECT_TRUE(r.isFailure());
    EXPECT_FALSE(r.isSuccess());
}

TEST(VoidResultTest, MapFromVoid) {
    auto r = Result<void, IOError>::success();
    auto mapped = r.map([]() { return 42; });
    ASSERT_TRUE(mapped.isSuccess());
    EXPECT_EQ(mapped.value(), 42);
}

TEST(VoidResultTest, FlatMapFromVoid) {
    auto r = Result<void, IOError>::success();
    auto chained = r.flatMap([]() { return Result<int, NotFound>::success(99); });
    ASSERT_TRUE(chained.isSuccess());
    EXPECT_EQ(chained.value(), 99);
}

TEST(VoidResultTest, MatchVoid) {
    auto ok = Result<void, IOError>::success();
    auto result = ok.match([]() { return std::string("ok"); }, [](OneOf<IOError>) { return std::string("err"); });
    EXPECT_EQ(result, "ok");

    auto err = Result<void, IOError>::failure(IOError{"/x", 1, {"fail"}});
    result = err.match([]() { return std::string("ok"); }, [](OneOf<IOError>) { return std::string("err"); });
    EXPECT_EQ(result, "err");
}

// --- VoidNeverResultTest suite: Result<void, Never> ---

TEST(VoidNeverResultTest, AlwaysSuccess) {
    auto r = Result<void, Never>::success();
    EXPECT_TRUE(r.isSuccess());
    EXPECT_FALSE(r.isFailure());
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST(VoidNeverResultTest, MatchCallsOnSuccess) {
    auto r = Result<void, Never>::success();
    auto result = r.match([]() { return 1; }, [](auto) { return -1; });
    EXPECT_EQ(result, 1);
}

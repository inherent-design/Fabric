#include "fabric/fx/Error.hh"
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>

using namespace fabric::fx;

// -- TaggedError concept checks -----------------------------------------------

struct PlainStruct {
    int value;
};

TEST(TaggedErrorTest, IOErrorSatisfiesConcept) {
    static_assert(TaggedError<IOError>);
}

TEST(TaggedErrorTest, StateErrorSatisfiesConcept) {
    static_assert(TaggedError<StateError>);
}

TEST(TaggedErrorTest, NotFoundSatisfiesConcept) {
    static_assert(TaggedError<NotFound>);
}

TEST(TaggedErrorTest, ConcurrencyErrorSatisfiesConcept) {
    static_assert(TaggedError<ConcurrencyError>);
}

TEST(TaggedErrorTest, NonTaggedTypeDoesNotSatisfy) {
    static_assert(!TaggedError<PlainStruct>);
    static_assert(!TaggedError<int>);
    static_assert(!TaggedError<std::string>);
}

// -- IOError ------------------------------------------------------------------

TEST(IOErrorTest, TagValue) {
    EXPECT_EQ(IOError::K_TAG, "io");
}

TEST(IOErrorTest, FieldAccess) {
    IOError err{.path = "/tmp/missing.dat", .code = 2, .ctx = ErrorContext("file not found")};

    EXPECT_EQ(err.path, "/tmp/missing.dat");
    EXPECT_EQ(err.code, 2);
    EXPECT_EQ(err.ctx.message, "file not found");
}

TEST(IOErrorTest, MoveConstruction) {
    IOError src{.path = "/tmp/data.bin", .code = 13, .ctx = ErrorContext("permission denied")};
    IOError dst(std::move(src));

    EXPECT_EQ(dst.path, "/tmp/data.bin");
    EXPECT_EQ(dst.code, 13);
    EXPECT_EQ(dst.ctx.message, "permission denied");
}

// -- StateError ---------------------------------------------------------------

TEST(StateErrorTest, TagValue) {
    EXPECT_EQ(StateError::K_TAG, "state");
}

TEST(StateErrorTest, FieldAccess) {
    StateError err{.expected = "Active", .actual = "Loading", .ctx = ErrorContext("bad transition")};

    EXPECT_EQ(err.expected, "Active");
    EXPECT_EQ(err.actual, "Loading");
    EXPECT_EQ(err.ctx.message, "bad transition");
}

// -- NotFound -----------------------------------------------------------------

TEST(NotFoundTest, TagValue) {
    EXPECT_EQ(NotFound::K_TAG, "not_found");
}

TEST(NotFoundTest, FieldAccess) {
    NotFound err{.key = "chunk:3:7:1", .ctx = ErrorContext("lookup miss")};

    EXPECT_EQ(err.key, "chunk:3:7:1");
    EXPECT_EQ(err.ctx.message, "lookup miss");
}

// -- ConcurrencyError ---------------------------------------------------------

TEST(ConcurrencyErrorTest, TagValue) {
    EXPECT_EQ(ConcurrencyError::K_TAG, "concurrency");
}

TEST(ConcurrencyErrorTest, FieldAccess) {
    ConcurrencyError err{.ctx = ErrorContext("deadlock detected")};

    EXPECT_EQ(err.ctx.message, "deadlock detected");
}

// -- ErrorContext -------------------------------------------------------------

TEST(ErrorContextTest, CapturesMessage) {
    ErrorContext ctx("something broke");
    EXPECT_EQ(ctx.message, "something broke");
}

TEST(ErrorContextTest, CapturesSourceLocation) {
    auto loc = std::source_location::current();
    ErrorContext ctx("manual location", loc);

    EXPECT_EQ(ctx.location.line(), loc.line());
    EXPECT_EQ(ctx.location.column(), loc.column());
}

TEST(ErrorContextTest, DefaultLocationIsCallerSite) {
    auto before = std::source_location::current();
    ErrorContext ctx("auto location");
    auto after = std::source_location::current();

    EXPECT_GE(ctx.location.line(), before.line());
    EXPECT_LE(ctx.location.line(), after.line());
}

#include "fabric/core/CompilerHints.hh"
#include "fabric/fx/WorldContext.hh"
#include "fabric/fx/WorldOps.hh"

#include <gtest/gtest.h>
#include <string>

namespace {

struct Ping {
    int value;
    static constexpr bool K_IS_SYNC = true;
    using Returns = int;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

struct Echo {
    std::string msg;
    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

struct MockSession {
    int lastPing = 0;
    std::string lastEcho;

    FABRIC_ALWAYS_INLINE int resolve(const Ping& op) {
        lastPing = op.value;
        return op.value * 2;
    }

    void submit(Echo op) { lastEcho = std::move(op.msg); }
};

static_assert(fabric::fx::SyncReadOp<Ping>);
static_assert(fabric::fx::AsyncMutationOp<Echo>);
static_assert(fabric::fx::Resolves<MockSession, Ping>);
static_assert(fabric::fx::Accepts<MockSession, Echo>);

} // namespace

TEST(WorldContextMockTest, ResolveReturnsMappedValue) {
    MockSession session;
    fabric::fx::WorldContext<MockSession> ctx(session);
    EXPECT_EQ(ctx.resolve(Ping{21}), 42);
    EXPECT_EQ(session.lastPing, 21);
}

TEST(WorldContextMockTest, SubmitForwardsOp) {
    MockSession session;
    fabric::fx::WorldContext<MockSession> ctx(session);
    ctx.submit(Echo{"hello"});
    EXPECT_EQ(session.lastEcho, "hello");
}

TEST(WorldContextMockTest, SessionEscapeHatch) {
    MockSession session;
    fabric::fx::WorldContext<MockSession> ctx(session);
    ctx.session().lastPing = 99;
    EXPECT_EQ(session.lastPing, 99);
}

TEST(WorldContextMockTest, ConstSessionAccess) {
    MockSession session;
    session.lastPing = 7;
    fabric::fx::WorldContext<MockSession> ctx(session);
    const auto& cctx = ctx;
    EXPECT_EQ(cctx.session().lastPing, 7);
}

TEST(WorldContextMockTest, MultipleResolvesAccumulate) {
    MockSession session;
    fabric::fx::WorldContext<MockSession> ctx(session);
    ctx.resolve(Ping{1});
    ctx.resolve(Ping{2});
    ctx.resolve(Ping{3});
    EXPECT_EQ(session.lastPing, 3);
}

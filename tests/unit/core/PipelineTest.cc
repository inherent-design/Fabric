#include "fabric/core/Pipeline.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

using namespace fabric;

struct TestContext {
    std::vector<std::string> log;
    int value = 0;
};

TEST(PipelineTest, ExecuteInPriorityOrder) {
    Pipeline<TestContext> pipeline;
    pipeline.addHandler(
        "second",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("B");
            next();
        },
        10);

    pipeline.addHandler(
        "first",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("A");
            next();
        },
        1);

    pipeline.addHandler(
        "third",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("C");
            next();
        },
        20);

    TestContext ctx;
    pipeline.execute(ctx);

    ASSERT_EQ(ctx.log.size(), 3u);
    EXPECT_EQ(ctx.log[0], "A");
    EXPECT_EQ(ctx.log[1], "B");
    EXPECT_EQ(ctx.log[2], "C");
}

TEST(PipelineTest, StableOrderForEqualPriority) {
    Pipeline<TestContext> pipeline;
    pipeline.addHandler(
        "X",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("X");
            next();
        },
        0);

    pipeline.addHandler(
        "Y",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("Y");
            next();
        },
        0);

    pipeline.addHandler(
        "Z",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("Z");
            next();
        },
        0);

    TestContext ctx;
    pipeline.execute(ctx);

    ASSERT_EQ(ctx.log.size(), 3u);
    EXPECT_EQ(ctx.log[0], "X");
    EXPECT_EQ(ctx.log[1], "Y");
    EXPECT_EQ(ctx.log[2], "Z");
}

TEST(PipelineTest, ShortCircuit) {
    Pipeline<TestContext> pipeline;
    pipeline.addHandler(
        "passes",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("A");
            next();
        },
        0);

    pipeline.addHandler(
        "blocks",
        [](TestContext& ctx, auto /*next*/) {
            ctx.log.push_back("B");
            // Does not call next(): short-circuits
        },
        1);

    pipeline.addHandler(
        "skipped",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("C");
            next();
        },
        2);

    TestContext ctx;
    pipeline.execute(ctx);

    ASSERT_EQ(ctx.log.size(), 2u);
    EXPECT_EQ(ctx.log[0], "A");
    EXPECT_EQ(ctx.log[1], "B");
}

TEST(PipelineTest, ContextModificationPropagates) {
    Pipeline<TestContext> pipeline;
    pipeline.addHandler(
        [](TestContext& ctx, auto next) {
            ctx.value = 10;
            next();
        },
        0);

    pipeline.addHandler(
        [](TestContext& ctx, auto next) {
            ctx.value *= 2;
            next();
        },
        1);

    pipeline.addHandler(
        [](TestContext& ctx, auto next) {
            ctx.value += 5;
            next();
        },
        2);

    TestContext ctx;
    pipeline.execute(ctx);
    EXPECT_EQ(ctx.value, 25); // (10 * 2) + 5
}

TEST(PipelineTest, RemoveNamedHandler) {
    Pipeline<TestContext> pipeline;
    pipeline.addHandler(
        "keep",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("kept");
            next();
        },
        0);

    pipeline.addHandler(
        "remove-me",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("removed");
            next();
        },
        1);

    EXPECT_EQ(pipeline.handlerCount(), 2u);
    EXPECT_TRUE(pipeline.removeHandler("remove-me"));
    EXPECT_EQ(pipeline.handlerCount(), 1u);

    TestContext ctx;
    pipeline.execute(ctx);
    ASSERT_EQ(ctx.log.size(), 1u);
    EXPECT_EQ(ctx.log[0], "kept");
}

TEST(PipelineTest, RemoveNonexistentReturnsFalse) {
    Pipeline<TestContext> pipeline;
    EXPECT_FALSE(pipeline.removeHandler("ghost"));
}

TEST(PipelineTest, ExceptionPropagation) {
    Pipeline<TestContext> pipeline;
    pipeline.addHandler(
        "ok",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("ok");
            next();
        },
        0);

    pipeline.addHandler("boom", [](TestContext&, auto) { throw std::runtime_error("handler error"); }, 1);

    pipeline.addHandler(
        "never",
        [](TestContext& ctx, auto next) {
            ctx.log.push_back("never");
            next();
        },
        2);

    TestContext ctx;
    EXPECT_THROW(pipeline.execute(ctx), std::runtime_error);
    ASSERT_EQ(ctx.log.size(), 1u);
    EXPECT_EQ(ctx.log[0], "ok");
}

TEST(PipelineTest, EmptyPipeline) {
    Pipeline<TestContext> pipeline;
    TestContext ctx;
    pipeline.execute(ctx); // should not crash
    EXPECT_EQ(pipeline.handlerCount(), 0u);
}

TEST(PipelineTest, HandlerCount) {
    Pipeline<TestContext> pipeline;
    EXPECT_EQ(pipeline.handlerCount(), 0u);
    pipeline.addHandler([](TestContext&, auto next) { next(); });
    EXPECT_EQ(pipeline.handlerCount(), 1u);
    pipeline.addHandler("named", [](TestContext&, auto next) { next(); });
    EXPECT_EQ(pipeline.handlerCount(), 2u);
}

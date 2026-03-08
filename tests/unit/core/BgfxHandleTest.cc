#include "fabric/core/BgfxHandle.hh"
#include "fixtures/BgfxNoopFixture.hh"
#include <bgfx/bgfx.h>
#include <gtest/gtest.h>

using namespace fabric;
using fabric::test::BgfxNoopFixture;

// -- Structural tests (no bgfx context needed) --------------------------------

TEST(BgfxHandleStructural, DefaultConstructedIsInvalid) {
    BgfxHandle<bgfx::UniformHandle> h;
    EXPECT_FALSE(h.isValid());
    EXPECT_EQ(h.get().idx, bgfx::kInvalidHandle);
}

TEST(BgfxHandleStructural, WorksWithMultipleHandleTypes) {
    BgfxHandle<bgfx::ProgramHandle> p;
    BgfxHandle<bgfx::TextureHandle> t;
    BgfxHandle<bgfx::FrameBufferHandle> f;
    BgfxHandle<bgfx::VertexBufferHandle> v;
    EXPECT_FALSE(p.isValid());
    EXPECT_FALSE(t.isValid());
    EXPECT_FALSE(f.isValid());
    EXPECT_FALSE(v.isValid());
}

// -- RAII tests (bgfx noop context required) ----------------------------------

TEST_F(BgfxNoopFixture, BgfxHandle_ExplicitConstruction) {
    auto raw = bgfx::createUniform("bh_a", bgfx::UniformType::Vec4);
    uint16_t idx = raw.idx;
    BgfxHandle<bgfx::UniformHandle> h(raw);

    EXPECT_TRUE(h.isValid());
    EXPECT_EQ(h.get().idx, idx);
}

TEST_F(BgfxNoopFixture, BgfxHandle_MoveConstructionTransfersOwnership) {
    BgfxHandle<bgfx::UniformHandle> a(bgfx::createUniform("bh_b", bgfx::UniformType::Vec4));
    uint16_t idx = a.get().idx;

    BgfxHandle<bgfx::UniformHandle> b(std::move(a));

    EXPECT_TRUE(b.isValid());
    EXPECT_EQ(b.get().idx, idx);
    EXPECT_FALSE(a.isValid());
}

TEST_F(BgfxNoopFixture, BgfxHandle_MoveAssignmentTransfersOwnership) {
    BgfxHandle<bgfx::UniformHandle> a(bgfx::createUniform("bh_c", bgfx::UniformType::Vec4));
    BgfxHandle<bgfx::UniformHandle> b(bgfx::createUniform("bh_d", bgfx::UniformType::Vec4));
    uint16_t idxA = a.get().idx;

    b = std::move(a);

    EXPECT_TRUE(b.isValid());
    EXPECT_EQ(b.get().idx, idxA);
    EXPECT_FALSE(a.isValid());
}

TEST_F(BgfxNoopFixture, BgfxHandle_SelfMoveAssignmentIsSafe) {
    BgfxHandle<bgfx::UniformHandle> a(bgfx::createUniform("bh_e", bgfx::UniformType::Vec4));
    uint16_t idx = a.get().idx;

    a = std::move(a);

    EXPECT_TRUE(a.isValid());
    EXPECT_EQ(a.get().idx, idx);
}

TEST_F(BgfxNoopFixture, BgfxHandle_ResetDestroysOldAndStoresNew) {
    BgfxHandle<bgfx::UniformHandle> h(bgfx::createUniform("bh_f", bgfx::UniformType::Vec4));
    auto newRaw = bgfx::createUniform("bh_g", bgfx::UniformType::Vec4);
    uint16_t newIdx = newRaw.idx;

    h.reset(newRaw);

    EXPECT_TRUE(h.isValid());
    EXPECT_EQ(h.get().idx, newIdx);
}

TEST_F(BgfxNoopFixture, BgfxHandle_ResetToDefaultInvalidates) {
    BgfxHandle<bgfx::UniformHandle> h(bgfx::createUniform("bh_h", bgfx::UniformType::Vec4));
    EXPECT_TRUE(h.isValid());

    h.reset();

    EXPECT_FALSE(h.isValid());
}

TEST_F(BgfxNoopFixture, BgfxHandle_ReleaseRelinquishesOwnership) {
    BgfxHandle<bgfx::UniformHandle> h(bgfx::createUniform("bh_i", bgfx::UniformType::Vec4));
    uint16_t idx = h.get().idx;

    auto raw = h.release();

    EXPECT_EQ(raw.idx, idx);
    EXPECT_FALSE(h.isValid());

    bgfx::destroy(raw);
}

TEST_F(BgfxNoopFixture, BgfxHandle_ComparisonOperators) {
    BgfxHandle<bgfx::UniformHandle> a(bgfx::createUniform("bh_j", bgfx::UniformType::Vec4));
    BgfxHandle<bgfx::UniformHandle> b(bgfx::createUniform("bh_k", bgfx::UniformType::Vec4));
    BgfxHandle<bgfx::UniformHandle> c;

    EXPECT_NE(a, b);
    EXPECT_NE(a, c);
    EXPECT_EQ(a, a);
}

TEST_F(BgfxNoopFixture, BgfxHandle_DestructorDestroysValidHandle) {
    { BgfxHandle<bgfx::UniformHandle> h(bgfx::createUniform("bh_l", bgfx::UniformType::Vec4)); }
    SUCCEED();
}

TEST_F(BgfxNoopFixture, BgfxHandle_DestructorOnInvalidIsNoOp) {
    { BgfxHandle<bgfx::UniformHandle> h; }
    SUCCEED();
}

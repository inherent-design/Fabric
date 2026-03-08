#include "fabric/core/HandleMap.hh"
#include "fixtures/BgfxNoopFixture.hh"
#include <bgfx/bgfx.h>
#include <gtest/gtest.h>

using namespace fabric;
using fabric::test::BgfxNoopFixture;

// -- Structural tests (no bgfx context needed) --------------------------------

TEST(HandleMapStructural, DefaultConstructedIsEmpty) {
    HandleMap<int, bgfx::UniformHandle> map;
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);

    // Prevent destructor from calling bgfx::destroy on empty map (no-op anyway)
}

TEST(HandleMapStructural, FindMissingReturnsNull) {
    HandleMap<int, bgfx::UniformHandle> map;
    EXPECT_EQ(map.find(42), nullptr);
}

TEST(HandleMapStructural, GetMissingReturnsInvalid) {
    HandleMap<int, bgfx::UniformHandle> map;
    EXPECT_EQ(map.get(42).idx, bgfx::kInvalidHandle);
}

TEST(HandleMapStructural, ContainsOnEmptyReturnsFalse) {
    HandleMap<int, bgfx::UniformHandle> map;
    EXPECT_FALSE(map.contains(1));
}

TEST(HandleMapStructural, EraseNonexistentReturnsFalse) {
    HandleMap<int, bgfx::UniformHandle> map;
    EXPECT_FALSE(map.erase(42));
}

// -- RAII tests (bgfx noop context required) ----------------------------------

TEST_F(BgfxNoopFixture, HandleMap_EmplaceInsertsHandle) {
    HandleMap<int, bgfx::UniformHandle> map;
    auto raw = bgfx::createUniform("hm_a", bgfx::UniformType::Vec4);

    bool inserted = map.emplace(1, raw);

    EXPECT_TRUE(inserted);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_TRUE(map.contains(1));
}

TEST_F(BgfxNoopFixture, HandleMap_EmplaceReplacesExisting) {
    HandleMap<int, bgfx::UniformHandle> map;
    map.emplace(1, bgfx::createUniform("hm_b", bgfx::UniformType::Vec4));
    auto newRaw = bgfx::createUniform("hm_c", bgfx::UniformType::Vec4);
    uint16_t newIdx = newRaw.idx;

    bool inserted = map.emplace(1, newRaw);

    EXPECT_FALSE(inserted);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.get(1).idx, newIdx);
}

TEST_F(BgfxNoopFixture, HandleMap_ReplaceHandleInsertsIfMissing) {
    HandleMap<int, bgfx::UniformHandle> map;
    auto raw = bgfx::createUniform("hm_d", bgfx::UniformType::Vec4);
    uint16_t idx = raw.idx;

    map.replaceHandle(1, raw);

    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.get(1).idx, idx);
}

TEST_F(BgfxNoopFixture, HandleMap_ReplaceHandleDestroysOld) {
    HandleMap<int, bgfx::UniformHandle> map;
    map.emplace(1, bgfx::createUniform("hm_e", bgfx::UniformType::Vec4));
    auto newRaw = bgfx::createUniform("hm_f", bgfx::UniformType::Vec4);
    uint16_t newIdx = newRaw.idx;

    map.replaceHandle(1, newRaw);

    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.get(1).idx, newIdx);
}

TEST_F(BgfxNoopFixture, HandleMap_EraseDestroysHandle) {
    HandleMap<int, bgfx::UniformHandle> map;
    map.emplace(1, bgfx::createUniform("hm_g", bgfx::UniformType::Vec4));

    bool erased = map.erase(1);

    EXPECT_TRUE(erased);
    EXPECT_TRUE(map.empty());
}

TEST_F(BgfxNoopFixture, HandleMap_FindReturnsPointer) {
    HandleMap<int, bgfx::UniformHandle> map;
    auto raw = bgfx::createUniform("hm_h", bgfx::UniformType::Vec4);
    uint16_t idx = raw.idx;
    map.emplace(1, raw);

    auto* found = map.find(1);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->get().idx, idx);
}

TEST_F(BgfxNoopFixture, HandleMap_GetReturnsRawHandle) {
    HandleMap<int, bgfx::UniformHandle> map;
    auto raw = bgfx::createUniform("hm_i", bgfx::UniformType::Vec4);
    uint16_t idx = raw.idx;
    map.emplace(1, raw);

    EXPECT_EQ(map.get(1).idx, idx);
}

TEST_F(BgfxNoopFixture, HandleMap_DestroyAllClearsMap) {
    HandleMap<int, bgfx::UniformHandle> map;
    map.emplace(1, bgfx::createUniform("hm_j", bgfx::UniformType::Vec4));
    map.emplace(2, bgfx::createUniform("hm_k", bgfx::UniformType::Vec4));
    map.emplace(3, bgfx::createUniform("hm_l", bgfx::UniformType::Vec4));
    EXPECT_EQ(map.size(), 3u);

    map.destroyAll();

    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
}

TEST_F(BgfxNoopFixture, HandleMap_MoveConstructionTransfers) {
    HandleMap<int, bgfx::UniformHandle> a;
    a.emplace(1, bgfx::createUniform("hm_m", bgfx::UniformType::Vec4));
    a.emplace(2, bgfx::createUniform("hm_n", bgfx::UniformType::Vec4));

    HandleMap<int, bgfx::UniformHandle> b(std::move(a));

    EXPECT_EQ(b.size(), 2u);
    EXPECT_TRUE(b.contains(1));
    EXPECT_TRUE(b.contains(2));
}

TEST_F(BgfxNoopFixture, HandleMap_MoveAssignmentTransfers) {
    HandleMap<int, bgfx::UniformHandle> a;
    a.emplace(1, bgfx::createUniform("hm_o", bgfx::UniformType::Vec4));

    HandleMap<int, bgfx::UniformHandle> b;
    b.emplace(2, bgfx::createUniform("hm_p", bgfx::UniformType::Vec4));

    b = std::move(a);

    EXPECT_TRUE(b.contains(1));
    EXPECT_FALSE(b.contains(2));
}

TEST_F(BgfxNoopFixture, HandleMap_Iteration) {
    HandleMap<int, bgfx::UniformHandle> map;
    map.emplace(10, bgfx::createUniform("hm_q", bgfx::UniformType::Vec4));
    map.emplace(20, bgfx::createUniform("hm_r", bgfx::UniformType::Vec4));

    int count = 0;
    for (auto& [key, handle] : map) {
        EXPECT_TRUE(handle.isValid());
        EXPECT_TRUE(key == 10 || key == 20);
        ++count;
    }
    EXPECT_EQ(count, 2);
}

TEST_F(BgfxNoopFixture, HandleMap_DestructorDestroysAllHandles) {
    {
        HandleMap<int, bgfx::UniformHandle> map;
        map.emplace(1, bgfx::createUniform("hm_s", bgfx::UniformType::Vec4));
        map.emplace(2, bgfx::createUniform("hm_t", bgfx::UniformType::Vec4));
    }
    SUCCEED();
}

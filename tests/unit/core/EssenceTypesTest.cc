#include "recurse/components/EssenceTypes.hh"
#include <gtest/gtest.h>

using namespace recurse;

TEST(EssenceTest, PureEssenceDominant) {
    EXPECT_EQ(Essence::pureOrder().dominant(), EssenceType::Order);
    EXPECT_EQ(Essence::pureChaos().dominant(), EssenceType::Chaos);
    EXPECT_EQ(Essence::pureLife().dominant(), EssenceType::Life);
    EXPECT_EQ(Essence::pureDecay().dominant(), EssenceType::Decay);
}

TEST(EssenceTest, MixedEssenceDominant) {
    Essence e{0.1f, 0.6f, 0.2f, 0.1f};
    EXPECT_EQ(e.dominant(), EssenceType::Chaos);

    Essence e2{0.5f, 0.1f, 0.3f, 0.1f};
    EXPECT_EQ(e2.dominant(), EssenceType::Order);
}

TEST(EssenceTest, LowValuesReturnNone) {
    Essence e{0.1f, 0.1f, 0.1f, 0.1f}; // All below threshold
    EXPECT_EQ(e.dominant(0.3f), EssenceType::None);
}

TEST(EssenceTest, BlendWorks) {
    Essence a = Essence::pureOrder();
    Essence b = Essence::pureChaos();
    Essence c = a.blend(b, 0.5f);

    EXPECT_FLOAT_EQ(c.order, 0.5f);
    EXPECT_FLOAT_EQ(c.chaos, 0.5f);
    EXPECT_FLOAT_EQ(c.life, 0.0f);
    EXPECT_FLOAT_EQ(c.decay, 0.0f);
}

TEST(EssenceTest, GetSetByType) {
    Essence e = Essence::neutral();
    e.set(EssenceType::Life, 0.9f);
    EXPECT_FLOAT_EQ(e.get(EssenceType::Life), 0.9f);
}

TEST(ArchetypeTest, StoneGolemMatchesEssence) {
    Essence e{0.8f, 0.0f, 0.0f, 0.2f};
    EXPECT_TRUE(Archetypes::StoneGolem.matchesEssence(e));
}

TEST(ArchetypeTest, FindArchetypeByEssence) {
    // Use an essence clearly closer to Stone Golem (0.8, 0, 0, 0.2)
    Essence e{0.78f, 0.02f, 0.0f, 0.20f};
    const NPCArchetype* arch = Archetypes::byEssence(e);
    ASSERT_NE(arch, nullptr);
    EXPECT_STREQ(arch->name, "Stone Golem");
}

TEST(ArchetypeTest, FindArchetypeByName) {
    const NPCArchetype* arch = Archetypes::byName("Treant");
    ASSERT_NE(arch, nullptr);
    EXPECT_FLOAT_EQ(arch->essenceBase.life, 0.8f);
}

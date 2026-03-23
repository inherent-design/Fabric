#include "recurse/simulation/WorldRuleEngine.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class WorldRuleEngineTest : public ::testing::Test {
  protected:
    WorldRuleEngine engine;
};

// 1. Engine has 8 rules after construction.
TEST_F(WorldRuleEngineTest, DefaultRuleCount) {
    EXPECT_EQ(engine.ruleCount(), 8u);
}

// 2. Query with WATER essence, self-transform, Liquid phase, temp=80 returns freeze rule.
TEST_F(WorldRuleEngineTest, WaterFreezeLookup) {
    std::vector<WorldRule> results;
    engine.query(4, 255, Phase::Liquid, 80, results);
    ASSERT_FALSE(results.empty());
    // Find the freeze rule (resultEssenceA == 6, ICE)
    bool found = false;
    for (const auto& r : results) {
        if (r.resultEssenceA == 6 && r.resultPhaseA == Phase::Solid) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// 3. Query with WATER at temp=95 does NOT return freeze rule (R1 requires temp <= 90).
TEST_F(WorldRuleEngineTest, WaterNoFreezeAboveThreshold) {
    std::vector<WorldRule> results;
    engine.query(4, 255, Phase::Liquid, 95, results);
    for (const auto& r : results) {
        // No rule should produce ICE at this temperature
        EXPECT_NE(r.resultEssenceA, 6) << "Freeze rule should not match at temp=95 (R1 requires temp <= 90)";
    }
}

// 4. Query with ICE, self-transform, Solid, temp=100 returns thaw rule.
TEST_F(WorldRuleEngineTest, IceThawLookup) {
    std::vector<WorldRule> results;
    engine.query(6, 255, Phase::Solid, 100, results);
    ASSERT_FALSE(results.empty());
    bool found = false;
    for (const auto& r : results) {
        if (r.resultEssenceA == 4 && r.resultPhaseA == Phase::Liquid) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// 5. Query with WATER + MAGMA contact returns R7.
TEST_F(WorldRuleEngineTest, WaterMagmaContact) {
    std::vector<WorldRule> results;
    engine.query(4, 11, Phase::Liquid, 100, results);
    ASSERT_FALSE(results.empty());
    bool found = false;
    for (const auto& r : results) {
        if (r.essenceIdxB == 11 && r.resultEssenceA == 1 && r.resultEssenceB == 1) {
            found = true;
            EXPECT_EQ(r.resultPhaseA, Phase::Solid);
            EXPECT_EQ(r.resultPhaseB, Phase::Solid);
            EXPECT_EQ(r.probability, 255);
            break;
        }
    }
    EXPECT_TRUE(found);
}

// 6. SAND at temp=150 returns no rules; at temp=200 returns vitrify rule.
TEST_F(WorldRuleEngineTest, TemperatureGating) {
    std::vector<WorldRule> belowThreshold;
    engine.query(3, 255, Phase::Powder, 150, belowThreshold);
    EXPECT_TRUE(belowThreshold.empty());

    std::vector<WorldRule> aboveThreshold;
    engine.query(3, 255, Phase::Powder, 200, aboveThreshold);
    ASSERT_FALSE(aboveThreshold.empty());
    bool found = false;
    for (const auto& r : aboveThreshold) {
        if (r.resultEssenceA == 10 && r.resultPhaseA == Phase::Solid) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// 7. When multiple rules match, results are sorted by priority descending.
TEST_F(WorldRuleEngineTest, PriorityOrdering) {
    // WATER at temp=145 matches R3 (boil, priority=190, tMin=125) and R8 (near-heat, priority=185, tMin=141)
    std::vector<WorldRule> results;
    engine.query(4, 255, Phase::Liquid, 145, results);
    ASSERT_GE(results.size(), 2u);
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i - 1].priority, results[i].priority) << "Results must be sorted by priority descending";
    }
}

// 8. Add a custom rule, verify ruleCount increases and query returns it.
TEST_F(WorldRuleEngineTest, AddCustomRule) {
    size_t before = engine.ruleCount();
    WorldRule custom{};
    custom.essenceIdxA = 2; // DIRT
    custom.essenceIdxB = 255;
    custom.requiredPhaseA = Phase::Solid;
    custom.temperatureMin = 200;
    custom.temperatureMax = 255;
    custom.resultEssenceA = 3; // -> SAND
    custom.resultPhaseA = Phase::Powder;
    custom.probability = 255;
    custom.priority = 150;
    custom.tag = 0;

    engine.addRule(custom);
    EXPECT_EQ(engine.ruleCount(), before + 1);

    std::vector<WorldRule> results;
    engine.query(2, 255, Phase::Solid, 210, results);
    ASSERT_FALSE(results.empty());
    bool found = false;
    for (const auto& r : results) {
        if (r.essenceIdxA == 2 && r.resultEssenceA == 3 && r.resultPhaseA == Phase::Powder) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// 9. A rule with essenceIdxA=255 matches any self essence.
TEST_F(WorldRuleEngineTest, WildcardMatch) {
    WorldRule wildcard{};
    wildcard.essenceIdxA = 255;
    wildcard.essenceIdxB = 255;
    wildcard.requiredPhaseA = Phase::Unchanged; // don't care
    wildcard.temperatureMin = 0;
    wildcard.temperatureMax = 255;
    wildcard.resultEssenceA = 0;
    wildcard.resultPhaseA = Phase::Empty;
    wildcard.probability = 255;
    wildcard.priority = 100;
    wildcard.tag = 0;

    engine.addRule(wildcard);

    // Should match any essence
    std::vector<WorldRule> r1;
    engine.query(1, 255, Phase::Solid, 100, r1);
    bool found1 = false;
    for (const auto& r : r1) {
        if (r.essenceIdxA == 255 && r.priority == 100) {
            found1 = true;
            break;
        }
    }
    EXPECT_TRUE(found1);

    std::vector<WorldRule> r2;
    engine.query(99, 255, Phase::Liquid, 50, r2);
    bool found2 = false;
    for (const auto& r : r2) {
        if (r.essenceIdxA == 255 && r.priority == 100) {
            found2 = true;
            break;
        }
    }
    EXPECT_TRUE(found2);
}

// 10. Self-transform rules (essenceIdxB=255) do not match contact queries.
TEST_F(WorldRuleEngineTest, SelfTransformDoesNotMatchContact) {
    // Query water touching magma (neighborEssence=11)
    std::vector<WorldRule> results;
    engine.query(4, 11, Phase::Liquid, 80, results);
    for (const auto& r : results) {
        EXPECT_NE(r.essenceIdxB, 255)
            << "Self-transform rule (essenceIdxB=255) must not match contact query with neighborEssence=11";
    }
}

// 11. Contact rules (essenceIdxB=specific) do not match self-transform queries.
TEST_F(WorldRuleEngineTest, ContactDoesNotMatchSelfTransform) {
    // Query water self-transform (neighborEssence=255)
    std::vector<WorldRule> results;
    engine.query(4, 255, Phase::Liquid, 80, results);
    for (const auto& r : results) {
        if (r.essenceIdxB != 255) {
            ADD_FAILURE() << "Contact rule (essenceIdxB=" << static_cast<int>(r.essenceIdxB)
                          << ") must not match self-transform query with neighborEssence=255";
        }
    }
}

// 12. Verify K_MAX_TRANSFORMS_PER_CHUNK == 64.
TEST_F(WorldRuleEngineTest, BudgetCapConstant) {
    EXPECT_EQ(K_MAX_TRANSFORMS_PER_CHUNK, 64);
}

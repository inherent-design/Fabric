#include "recurse/simulation/WorldRuleEngine.hh"
#include "fabric/log/Log.hh"
#include <algorithm>

namespace recurse::simulation {

WorldRuleEngine::WorldRuleEngine() {
    // Material essenceIdx values (1:1 with MaterialId during migration):
    // AIR=0, STONE=1, DIRT=2, SAND=3, WATER=4, GRAVEL=5, ICE=6, GLASS=10, MAGMA=11

    // R1: Water freeze (temp <= 90, 50%)
    rules_.push_back({.essenceIdxA = 4,
                      .essenceIdxB = 255,
                      .requiredPhaseA = Phase::Liquid,
                      .temperatureMin = 0,
                      .temperatureMax = 90,
                      .resultEssenceA = 6,
                      .resultEssenceB = 255,
                      .resultPhaseA = Phase::Solid,
                      .resultPhaseB = Phase::Unchanged,
                      .resultTempA = 0,
                      .resultTempB = 0,
                      .probability = 128,
                      .priority = 200,
                      .tag = 0});

    // R2: Ice thaw (temp >= 92, 75%)
    rules_.push_back({.essenceIdxA = 6,
                      .essenceIdxB = 255,
                      .requiredPhaseA = Phase::Solid,
                      .temperatureMin = 92,
                      .temperatureMax = 255,
                      .resultEssenceA = 4,
                      .resultEssenceB = 255,
                      .resultPhaseA = Phase::Liquid,
                      .resultPhaseB = Phase::Unchanged,
                      .resultTempA = 0,
                      .resultTempB = 0,
                      .probability = 192,
                      .priority = 200,
                      .tag = 0});

    // R3: Water boil (temp >= 125, 25%)
    rules_.push_back({.essenceIdxA = 4,
                      .essenceIdxB = 255,
                      .requiredPhaseA = Phase::Liquid,
                      .temperatureMin = 125,
                      .temperatureMax = 255,
                      .resultEssenceA = 0,
                      .resultEssenceB = 255,
                      .resultPhaseA = Phase::Empty,
                      .resultPhaseB = Phase::Unchanged,
                      .resultTempA = 0,
                      .resultTempB = 0,
                      .probability = 64,
                      .priority = 190,
                      .tag = 0});

    // R4: Sand vitrify (temp >= 180, 12%)
    rules_.push_back({.essenceIdxA = 3,
                      .essenceIdxB = 255,
                      .requiredPhaseA = Phase::Powder,
                      .temperatureMin = 180,
                      .temperatureMax = 255,
                      .resultEssenceA = 10,
                      .resultEssenceB = 255,
                      .resultPhaseA = Phase::Solid,
                      .resultPhaseB = Phase::Unchanged,
                      .resultTempA = 0,
                      .resultTempB = 0,
                      .probability = 32,
                      .priority = 180,
                      .tag = 0});

    // R5: Stone melt (temp >= 196, 6%)
    rules_.push_back({.essenceIdxA = 1,
                      .essenceIdxB = 255,
                      .requiredPhaseA = Phase::Solid,
                      .temperatureMin = 196,
                      .temperatureMax = 255,
                      .resultEssenceA = 11,
                      .resultEssenceB = 255,
                      .resultPhaseA = Phase::Liquid,
                      .resultPhaseB = Phase::Unchanged,
                      .resultTempA = 0,
                      .resultTempB = 0,
                      .probability = 16,
                      .priority = 180,
                      .tag = 0});

    // R6: Magma cool (temp <= 194, 50%)
    rules_.push_back({.essenceIdxA = 11,
                      .essenceIdxB = 255,
                      .requiredPhaseA = Phase::Liquid,
                      .temperatureMin = 0,
                      .temperatureMax = 194,
                      .resultEssenceA = 1,
                      .resultEssenceB = 255,
                      .resultPhaseA = Phase::Solid,
                      .resultPhaseB = Phase::Unchanged,
                      .resultTempA = 0,
                      .resultTempB = 0,
                      .probability = 128,
                      .priority = 200,
                      .tag = 0});

    // R7: Water + Magma contact (any temp, 100%)
    rules_.push_back({.essenceIdxA = 4,
                      .essenceIdxB = 11,
                      .requiredPhaseA = Phase::Liquid,
                      .temperatureMin = 0,
                      .temperatureMax = 255,
                      .resultEssenceA = 1,
                      .resultEssenceB = 1,
                      .resultPhaseA = Phase::Solid,
                      .resultPhaseB = Phase::Solid,
                      .resultTempA = 150,
                      .resultTempB = 150,
                      .probability = 255,
                      .priority = 220,
                      .tag = 1});

    // R8: Near-heat evaporation (temp >= 141, 50%)
    rules_.push_back({.essenceIdxA = 4,
                      .essenceIdxB = 255,
                      .requiredPhaseA = Phase::Liquid,
                      .temperatureMin = 141,
                      .temperatureMax = 255,
                      .resultEssenceA = 0,
                      .resultEssenceB = 255,
                      .resultPhaseA = Phase::Empty,
                      .resultPhaseB = Phase::Unchanged,
                      .resultTempA = 0,
                      .resultTempB = 0,
                      .probability = 128,
                      .priority = 185,
                      .tag = 1});

    sortByPriority();

    FABRIC_LOG_INFO("WorldRuleEngine initialized: {} rules", rules_.size());
}

void WorldRuleEngine::addRule(WorldRule rule) {
    rules_.push_back(rule);
    sortByPriority();
}

void WorldRuleEngine::query(uint8_t selfEssence, uint8_t neighborEssence, Phase selfPhase, uint8_t temperature,
                            std::vector<WorldRule>& results) const {
    results.clear();
    for (const auto& rule : rules_) {
        if (matches(rule, selfEssence, neighborEssence, selfPhase, temperature)) {
            results.push_back(rule);
        }
    }
}

size_t WorldRuleEngine::ruleCount() const {
    return rules_.size();
}

std::span<const WorldRule> WorldRuleEngine::allRules() const {
    return rules_;
}

void WorldRuleEngine::sortByPriority() {
    std::sort(rules_.begin(), rules_.end(),
              [](const WorldRule& a, const WorldRule& b) { return a.priority > b.priority; });
}

bool WorldRuleEngine::matches(const WorldRule& rule, uint8_t selfEssence, uint8_t neighborEssence, Phase selfPhase,
                              uint8_t temperature) const {
    if (rule.essenceIdxA != selfEssence && rule.essenceIdxA != 255)
        return false;
    if (rule.essenceIdxB != neighborEssence)
        return false;
    if (rule.requiredPhaseA != Phase::Unchanged && rule.requiredPhaseA != selfPhase)
        return false;
    if (temperature < rule.temperatureMin || temperature > rule.temperatureMax)
        return false;
    return true;
}

} // namespace recurse::simulation

#include "fabric/core/Simulation.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <set>

namespace fabric {

void SimulationHarness::registerRule(const std::string& name, SimRule rule) {
    rules_.emplace_back(name, std::move(rule));
}

bool SimulationHarness::removeRule(const std::string& name) {
    auto it = std::find_if(rules_.begin(), rules_.end(),
                           [&](const auto& p) { return p.first == name; });
    if (it == rules_.end()) return false;
    rules_.erase(it);
    return true;
}

size_t SimulationHarness::ruleCount() const {
    return rules_.size();
}

void SimulationHarness::tick(double dt) {
    FABRIC_ZONE_SCOPED_N("Simulation::tick");

    if (rules_.empty()) return;

    // Merge active chunks from both fields, deduplicated and sorted
    std::set<std::tuple<int, int, int>> merged;
    for (auto& c : density_.grid().activeChunks()) merged.insert(c);
    for (auto& c : essence_.grid().activeChunks()) merged.insert(c);

    for (auto [cx, cy, cz] : merged) {
        int baseX = cx * kChunkSize;
        int baseY = cy * kChunkSize;
        int baseZ = cz * kChunkSize;
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int ly = 0; ly < kChunkSize; ++ly) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    int wx = baseX + lx;
                    int wy = baseY + ly;
                    int wz = baseZ + lz;
                    for (auto& [_, rule] : rules_) {
                        rule(density_, essence_, wx, wy, wz, dt);
                    }
                }
            }
        }
    }
}

DensityField& SimulationHarness::density() { return density_; }
const DensityField& SimulationHarness::density() const { return density_; }
EssenceField& SimulationHarness::essence() { return essence_; }
const EssenceField& SimulationHarness::essence() const { return essence_; }

} // namespace fabric

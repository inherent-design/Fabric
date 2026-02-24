#pragma once

#include "fabric/core/FieldLayer.hh"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace fabric {

using SimRule = std::function<void(DensityField&, EssenceField&, int x, int y, int z, double dt)>;

class SimulationHarness {
  public:
    SimulationHarness() = default;

    void registerRule(const std::string& name, SimRule rule);
    bool removeRule(const std::string& name);
    size_t ruleCount() const;

    void tick(double dt);

    DensityField& density();
    const DensityField& density() const;
    EssenceField& essence();
    const EssenceField& essence() const;

  private:
    DensityField density_;
    EssenceField essence_;
    std::vector<std::pair<std::string, SimRule>> rules_;
};

} // namespace fabric

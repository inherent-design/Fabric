#include "recurse/world/WorldGenerator.hh"

namespace recurse {

uint16_t WorldGenerator::sampleMaterial(int /*wx*/, int /*wy*/, int /*wz*/) const {
    return 0; // AIR; subclasses override with efficient point queries
}

} // namespace recurse

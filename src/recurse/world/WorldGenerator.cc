#include "recurse/world/WorldGenerator.hh"

namespace recurse {

uint16_t WorldGenerator::sampleMaterial(int /*wx*/, int /*wy*/, int /*wz*/) const {
    return 0; // AIR; subclasses override with efficient point queries
}

int WorldGenerator::maxSurfaceHeight(int /*cx*/, int /*cz*/) const {
    return 1024; // Conservative default: never skip for unknown generators
}

} // namespace recurse

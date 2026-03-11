#pragma once

namespace recurse {

/// Entities with StreamSource + fabric::Position contribute focal points
/// for ChunkStreamingManager (streaming) and PhysicsGameSystem (collision).
/// Opt-in per archetype; not all entities need streaming or collision.
struct StreamSource {
    int streamRadius = 0;
    int collisionRadius = 0;
};

} // namespace recurse

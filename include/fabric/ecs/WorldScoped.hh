#pragma once

namespace fabric {

/// Tag component for per-world ECS entities.
/// Entities tagged with WorldScoped are bulk-deleted on world transitions
/// via ecs_delete_with(world, ecs_id(WorldScoped)).
struct WorldScoped {};

} // namespace fabric

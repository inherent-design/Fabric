# Recurse Game Design Documents

Historical design documents from the original Recurse concept (GMTK Game Jam 2025, Godot 4.4). Code examples reference the original Godot architecture, not the current C++20/Fabric engine. For current engine docs, see `docs/ARCHITECTURE.md` and `CLAUDE.md`.

Some concepts carried forward into the codebase (material system, falling sand, world generation, chunk streaming, ECS, physics, audio). Others remain design intent only.

The semantic spatial graph concept was explored during early design but never implemented; it has been removed from these docs.

## File listing

| File | Contents |
|------|----------|
| `DESIGN_DOCUMENT.md` | Core game pillars, mechanics overview |
| `AESTHETIC.md` | Visual direction, art style |
| `GAME_LOOP.md` | Tick structure, phase pipeline |
| `PROJECT_PLAN.md` | Original development roadmap |
| `QUANTA_FIELDS.md` | Godot-era material/essence concept |
| `TRIBES.md` | Faction and tribe design |
| `2_CORE_SIMULATION/2.1_WORLD_STATE.md` | World state data model |
| `3_ENTITY_COMPONENT_SYSTEM/3.*.md` | ECS components, entities, systems overview |
| `systems/FUNCTION_META_SYSTEM.md` | Function language: composition, execution, mastery, progression |
| `systems/NPC_SYSTEM_DESIGN.md` | NPC consciousness model, factions, behavioral emergence |
| `systems/PROCEDURAL_WORLD_SYSTEM.md` | Godot-era function-based world manipulation |
| `reference/NPC_REFERENCE.md` | Per-NPC stat tables, equipment, behavioral patterns |

## Implemented systems (stub docs)

Code exists for these Recurse-specific systems; docs are scaffolded but incomplete.

| File | Contents |
|------|----------|
| `MATERIAL_SYSTEM.md` | Material palette, registry, phase/density properties |
| `FALLING_SAND.md` | Cellular automaton: powder, liquid, gas movement |
| `WORLD_GENERATION.md` | FastNoise2 terrain pipeline, biome rules |
| `PERSISTENCE.md` | Chunk save/load, serialization format |
| `CHUNK_STREAMING.md` | Load/unload lifecycle, streaming radius, budget |
| `MESHING.md` | Greedy meshing, merge keys, SnapMC stub |
| `PHYSICS.md` | Entity-vs-voxel collision, batch dispatch |
| `AUDIO.md` | Spatial audio, sound categories, mixing |

## Planned features

Stub docs for features required to ship but not yet implemented.

| File | Contents |
|------|----------|
| `planned/PLAYER_CONTROLLER.md` | Third-person movement, camera, input |
| `planned/ANIMATION.md` | Locomotion, combat, casting animations |
| `planned/SOUND_DESIGN.md` | Sound palette, music direction, ambient layers |
| `planned/ART_PIPELINE.md` | Voxel palettes, VFX, lighting, post-processing |
| `planned/UI_HUD.md` | Health/resource display, function selection, menus |
| `planned/NARRATIVE.md` | Environmental storytelling, loop structure, endings |
| `planned/PROGRESSION.md` | Function mastery, faction reputation, cross-loop persistence |

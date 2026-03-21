# Animation System

> **Not yet implemented.** Design placeholder for a confirmed game feature.

## Purpose

Character and NPC animation: locomotion blending, combat animations, function casting visual feedback, NPC behavioral animation states.

## Requirements

- Locomotion blend tree (idle, walk, run, jump, fall)
- Combat animations (attack, hit reaction, death)
- Function casting animations (charge, release, channel)
- NPC state-driven animation (idle, patrol, attack, transform)

## Open Design Questions

- Skeletal vs procedural animation approach
- Animation asset format and pipeline
- Blending strategy for simultaneous actions (move + cast)
- NPC transformation visual sequence (dissolve and reform per AESTHETIC.md)

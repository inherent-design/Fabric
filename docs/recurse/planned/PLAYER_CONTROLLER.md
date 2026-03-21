# Player Controller

> **Not yet designed.** Basic character and input infrastructure exists in the engine (`include/recurse/character/`, `include/recurse/input/`) but the gameplay-level controller (movement feel, camera behavior, function casting interface) is not yet specified.

## Purpose

Third-person character controller: movement, camera, input handling, function casting interface. The original design references "Spellbreak-level fluidity with Pseudoregalia precision."

## Requirements

- Smooth third-person camera with collision avoidance
- Ground movement, jumping, air control
- Function casting input (aim, select, fire)
- Jump buffering and coyote time

## Open Design Questions

- Camera style: over-the-shoulder vs centered third-person
- Movement speed and acceleration curves
- Input binding scheme (keyboard/mouse, gamepad)
- Integration with function system (hotbar, radial menu, or sequential)

# RECURSE - Game Design Document
*GMTK Game Jam 2025 - Theme: "Loop"*

## Core Concept

A 3D third-person action game where the player fights through their life experiences in reverse temporal order. The main character exists in a shattered snow globe (META-VERSE), reliving memories backwards through procedurally generated levels while fighting to change their fate using reality-manipulation functions.

**Core Loop**: Fight → Survive → Discover → Reset → Loop

## Aesthetic Vision

- **Visual Style**: Mirror's Edge minimalistic 3D environments with procedural generation
- **Architecture**: Clean geometric forms with strategic detail placement
- **Color Palette**: Stark whites and grays with accent colors for interactive elements
- **Atmosphere**: Dreamlike, surreal, introspective

## Technical Constraints

- **Engine**: Godot 4.4/4.5 beta
- **Perspective**: 3D third-person
- **Movement**: Spellbreak-level fluidity with Pseudoregalia precision
- **Experimentation**: Noita-like systemic interactions
- **Timeline**: 4 days total development

---

# Three-Phase Development Plan

## Phase 1: Bootstrapping (1-2 Days)
*Foundation Systems & Core Architecture*

### Core Engine Systems
- **Player Controller**: Third-person camera with smooth movement
- **Function System**: Basic reality-manipulation spell framework
- **NPC Base Classes**: Behavioral AI foundation with stats system
- **Procedural Generation**: Basic level generation with modular components
- **Combat System**: Health, damage, basic interactions

### Essential Infrastructure
- **Scene Management**: Level loading and transition system
- **Game State Manager**: Player progression and memory system
- **Audio Manager**: Dynamic audio system for functions and combat
- **Input System**: Responsive controls for movement and function casting
- **Debug Tools**: In-game console and parameter tweaking

### Minimum Viable Gameplay
- Player can move smoothly in 3D space
- Player can cast 3-5 basic functions
- NPCs spawn with basic AI behavior
- Simple procedural level generation
- Basic combat interactions work

### Success Criteria
- 30 seconds of core gameplay loop functional
- All foundational systems integrated
- No major technical blockers remain

---

## Phase 2: Incremental Development (1-2 Days)
*Content Creation & System Refinement*

### Content Expansion
- **Function Library**: Expand to 10-15 unique reality-manipulation functions
- **NPC Variety**: 8-10 different NPC types with faction behaviors
- **Level Generation**: 3-4 distinct level archetypes with procedural variation
- **Behavioral Complexity**: Advanced AI interactions and emergent behaviors

### Gameplay Polish
- **Combat Feel**: Damage feedback, hit reactions, screen shake
- **Movement Polish**: Jump buffering, coyote time, smooth interpolation
- **Function Interactions**: Systemic combinations between different functions
- **Difficulty Scaling**: Dynamic challenge based on player progression

### AI-Assisted Content Generation
- **Enemy Behavior Variants**: Generate and test multiple behavior configurations
- **Function Parameter Tuning**: Optimize costs, cooldowns, and effects
- **Level Layout Optimization**: Test and rank procedural generation seeds
- **Balance Testing**: Automated playtesting of combat scenarios

### Success Criteria
- 10-15 minutes of varied gameplay content
- All major systems feature-complete
- Emergent interactions create surprising moments
- Game feel approaches target quality

---

## Phase 3: Playtesting & Final Polish (1 Day)
*Optimization & User Experience*

### User Experience Polish
- **UI/UX**: Clean, minimal interface design
- **Onboarding**: Intuitive function discovery and control learning
- **Feedback Systems**: Clear visual/audio feedback for all interactions
- **Performance**: Stable 60fps on target hardware

### Balance & Tuning
- **Function Balance**: Cost/benefit optimization for all abilities
- **NPC Behavior**: Fine-tune aggression, cooperation, and threat assessment
- **Difficulty Curve**: Ensure appropriate challenge progression
- **Pacing**: Optimize level transitions and intensity curves

### Final Integration
- **Narrative Elements**: Environmental storytelling through level design
- **Loop Mechanics**: Ensure the "reset" mechanic feels meaningful
- **Bug Fixing**: Address critical gameplay issues
- **Build Optimization**: Final executable preparation

### Success Criteria
- 10-15 minutes of polished gameplay
- No game-breaking bugs
- Core theme of "loop" clearly communicated
- Submitted on time with documentation

---

# Core Gameplay Pillars

## 1. Reality Manipulation Functions
- **Philosophy**: Player controls matter and time through procedural generation
- **Implementation**: Function-based system with costs and cooldowns
- **Interaction**: Functions combine with environment and NPCs systemically

## 2. Emergent NPC Societies
- **Philosophy**: NPCs have agency and form dynamic relationships
- **Implementation**: Stat-based AI with faction behaviors and tribal dynamics
- **Interaction**: Player actions influence NPC society formation and hostility

## 3. Procedural Memory Landscapes
- **Philosophy**: Levels represent distorted life experiences
- **Implementation**: Modular generation with thematic consistency
- **Interaction**: Player exploration reveals environmental storytelling

## 4. Temporal Loop Structure
- **Philosophy**: Player fights backwards through time to change fate
- **Implementation**: Reverse chronological level progression with reset mechanic
- **Interaction**: Each loop provides new knowledge and strategic options

---

# Technical Architecture Overview

## Core Systems Integration
```
Game Manager
├── Scene Manager (Level transitions)
├── Player Controller (Movement + Functions)
├── NPC Manager (AI behaviors + Factions)
├── Procedural Generator (Level creation)
├── Audio Manager (Dynamic soundscape)
└── UI Manager (Minimal interface)
```

## Data-Driven Design
- **Functions**: JSON configuration files for easy iteration
- **NPCs**: Stat-based system with behavioral parameter tuning
- **Levels**: Modular prefab system with procedural assembly
- **Balance**: External configuration for rapid prototyping

## Performance Targets
- **Framerate**: Stable 60fps
- **Memory**: <2GB RAM usage
- **Loading**: <3 second level transitions
- **Responsiveness**: <50ms input latency

---

# Risk Assessment & Mitigation

## High Risk Areas
1. **Procedural Generation Complexity**: Scope creep in level generation
   - *Mitigation*: Start with simple modular assembly, iterate incrementally
2. **NPC AI Performance**: Complex behaviors causing frame drops
   - *Mitigation*: LOD system for AI updates, behavioral simplification at distance
3. **Function System Complexity**: Over-engineering the reality manipulation
   - *Mitigation*: Focus on 5-8 core functions, add complexity through combinations

## Medium Risk Areas
1. **Game Feel Polish**: Movement and combat responsiveness
   - *Mitigation*: Prioritize core player controller, test frequently
2. **Scope Management**: Feature creep threatening timeline
   - *Mitigation*: Strict prioritization, cut features before polish phase

## Low Risk Areas
1. **Technical Implementation**: Godot 4.x capabilities well-understood
2. **Art Asset Pipeline**: Minimal art requirements reduce production risk
3. **Audio Integration**: Simple audio needs with clear implementation path

---

# Success Metrics

## Technical Success
- Game runs without crashes for 15+ minute sessions
- All core systems integrated and functional
- Performance targets met on target hardware

## Design Success
- "Loop" theme clearly communicated through gameplay
- Player agency feels meaningful and impactful
- Emergent interactions create memorable moments

## Experience Success
- 10-15 minutes of engaging gameplay content
- Learning curve allows intuitive function discovery
- Replayability through procedural variation and systemic depth

---

*This design document serves as the foundation for "Recurse" development. Each phase builds systematically toward a polished game jam submission that explores themes of self-discovery, temporal manipulation, and emergent social dynamics.*
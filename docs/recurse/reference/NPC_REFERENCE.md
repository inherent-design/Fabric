# NPC Reference Guide - RECURSE
*Complete Character Database with Stats, Behaviors, and Faction Details*

## NPC Stat System Overview

**Core Stats** (All values -1.0 to 1.0 except where noted):
- **Fear/Courage**: Combat willingness and threat response
- **Empathy/Apathy**: Cooperation and altruism levels  
- **Health/Damage**: Current condition (0.0 to 1.0)
- **Meta-Capability**: Function usage ability (0.0 to 1.0)

**Derived Properties**:
- **Threat Assessment**: How dangerous they perceive the player
- **Faction Loyalty**: Strength of tribal bonds
- **Aggression Level**: Likelihood to initiate combat
- **Cooperation Chance**: Willingness to help others

---

# CIVILIAN NPCS (6 Types)

## 1. PEDESTRIAN_RUNNER
*Default civilian with varied behavior patterns*

**Base Stats:**
- **Fear/Courage**: -0.2 to 0.3 (slightly cautious to mildly brave)
- **Empathy/Apathy**: -0.1 to 0.4 (selfish to somewhat helpful)
- **Health/Damage**: 0.6 to 1.0 (healthy civilians)
- **Meta-Capability**: 0.0 to 0.1 (no function usage)

**Physical Properties:**
- **Max Health**: 40-60 HP
- **Movement Speed**: 3.5-4.2 m/s
- **Detection Range**: 8-12 meters
- **Memory Duration**: 30-45 seconds

**Behavioral Patterns:**
- **Default State**: Random wandering, simple activities
- **Player Spotted**: Assessment phase (2-4 seconds)
- **Low Threat**: Cautious observation, possible approach
- **High Threat**: Immediate flight, panic spreading

**Faction**: CIVILIAN
**Spawn Weight**: 40% (most common NPC)
**Special Abilities**: None
**Loot Drops**: Basic materials, small energy crystals

**Threat Response Matrix:**
```
Player Function Level → Response
0.0-0.2: Curious approach or ignore
0.3-0.4: Cautious observation  
0.5-0.6: Nervous retreat
0.7+: Panic flight, alert others
```

---

## 2. COP
*Law enforcement officer with protective instincts*

**Base Stats:**
- **Fear/Courage**: 0.2 to 0.7 (brave to heroic)
- **Empathy/Apathy**: 0.0 to 0.5 (neutral to protective)
- **Health/Damage**: 0.7 to 1.0 (well-conditioned)
- **Meta-Capability**: 0.1 to 0.3 (basic function recognition)

**Physical Properties:**
- **Max Health**: 80-120 HP
- **Movement Speed**: 4.0-4.8 m/s
- **Detection Range**: 15-20 meters
- **Memory Duration**: 60-90 seconds

**Equipment:**
- **Weapon**: Standard projectile weapon (25-35 damage)
- **Armor**: Light protection (-20% damage from physical)
- **Communication**: Can call for backup

**Behavioral Patterns:**
- **Default State**: Patrol patterns, investigate disturbances
- **Player Spotted**: Verbal warning first, then assessment
- **Function Usage Detected**: Investigative approach
- **Violence Witnessed**: Immediate intervention attempt

**Faction**: CIVILIAN (but cooperates with FBI_AGENTS)
**Spawn Weight**: 15% in urban areas, 5% elsewhere
**Special Abilities**: 
- **Backup Call**: Summons 1-2 additional cops (30% chance)
- **Restraint Protocol**: Attempts non-lethal takedown first

**Decision Tree:**
1. **Assess Situation**: Is player threatening civilians?
2. **Verbal Warning**: "Stop right there!" (50% chance player complies)
3. **Non-lethal Force**: Attempt to subdue without killing
4. **Lethal Force**: Only if player continues violence

---

## 3. HOMELESS
*Survival-focused individual with street wisdom*

**Base Stats:**
- **Fear/Courage**: -0.3 to 0.4 (varies widely)
- **Empathy/Apathy**: 0.2 to 0.8 (community-minded)
- **Health/Damage**: 0.3 to 0.7 (often injured/sick)
- **Meta-Capability**: 0.0 to 0.2 (survival instincts only)

**Physical Properties:**
- **Max Health**: 30-50 HP  
- **Movement Speed**: 2.8-3.5 m/s (slower due to condition)
- **Detection Range**: 12-18 meters (heightened awareness)
- **Memory Duration**: 90-120 seconds (remembers threats well)

**Behavioral Patterns:**
- **Default State**: Scavenging, avoiding authority figures
- **Player Approach**: Defensive but not immediately hostile
- **Resource Sharing**: Will trade information/items for help
- **Authority Figures**: Hides from cops, flees from FBI

**Faction**: HOMELESS_TRIBE (forms cooperative groups)
**Spawn Weight**: 20% in urban areas, 8% elsewhere
**Special Abilities**:
- **Scavenge**: Can find basic materials in empty areas
- **Street Knowledge**: Knows hidden paths and safe spots
- **Group Formation**: Spontaneously allies with other homeless NPCs

**Trading Behavior:**
- **Accepts**: Food, medicine, basic materials
- **Offers**: Information about area, hidden paths, warnings about dangers
- **Trust Building**: Repeated positive interactions increase cooperation

---

## 4. SOCCER_MOM  
*Protective parent figure with community focus*

**Base Stats:**
- **Fear/Courage**: -0.1 to 0.5 (protective instincts)
- **Empathy/Apathy**: 0.3 to 0.8 (highly empathetic)
- **Health/Damage**: 0.6 to 0.9 (healthy adult)
- **Meta-Capability**: 0.0 to 0.1 (no special abilities)

**Physical Properties:**
- **Max Health**: 50-70 HP
- **Movement Speed**: 3.2-4.0 m/s
- **Detection Range**: 10-15 meters
- **Memory Duration**: 45-60 seconds

**Behavioral Patterns:**
- **Default State**: Purposeful movement, checking on others
- **Protecting Others**: Will intervene to help other civilians
- **Player Assessment**: Judges based on player's treatment of others
- **Crisis Response**: Organizes other civilians for mutual aid

**Faction**: CIVILIAN (strong community bonds)  
**Spawn Weight**: 12% in suburban areas, 3% elsewhere
**Special Abilities**:
- **Rally Civilians**: Can organize nearby NPCs for collective action
- **First Aid**: Can heal other NPCs slightly
- **Moral Authority**: Other civilians listen to her warnings/advice

**Reputation System:**
- **Player helps civilians**: +2 reputation per incident
- **Player harms civilians**: -5 reputation per incident  
- **High reputation**: Will actively help player
- **Low reputation**: Organizes civilian resistance

---

# HOSTILE NPCS (3 Types)

## 5. BAD_COP
*Corrupt law enforcement with aggressive tendencies*

**Base Stats:**
- **Fear/Courage**: 0.3 to 0.8 (aggressive and confident)
- **Empathy/Apathy**: -0.6 to -0.2 (actively harmful)
- **Health/Damage**: 0.7 to 1.0 (well-equipped)
- **Meta-Capability**: 0.2 to 0.4 (enhanced law enforcement training)

**Physical Properties:**
- **Max Health**: 100-140 HP
- **Movement Speed**: 4.2-5.0 m/s
- **Detection Range**: 18-25 meters
- **Memory Duration**: 120+ seconds (persistent)

**Equipment:**
- **Weapon**: Heavy projectile weapon (35-50 damage)
- **Armor**: Medium protection (-35% physical damage)
- **Tactical Gear**: Grenades, restraints, communication device

**Behavioral Patterns:**
- **Default State**: Aggressive patrol, hassling civilians
- **Player Spotted**: Immediate hostile assessment
- **Function Usage**: Escalates to lethal force quickly  
- **Civilian Interaction**: Intimidation and abuse

**Faction**: Rogue CIVILIAN (operates independently)
**Spawn Weight**: 8% in urban areas, 3% elsewhere  
**Special Abilities**:
- **Intimidation**: Causes fear in nearby civilians
- **Combat Training**: +25% accuracy, faster reload
- **Persistence**: Continues hunting player longer than normal NPCs

**Escalation Pattern:**
1. **Spot Player**: Immediate aggressive posture
2. **Demand Compliance**: "Get on the ground!" (10% player compliance)
3. **Use Force**: Attacks within 3-5 seconds
4. **Call Backup**: 40% chance to summon another BAD_COP

---

## 6. METH_HEAD
*Erratic drug user with unpredictable behavior*

**Base Stats:**
- **Fear/Courage**: -0.4 to 0.9 (extremely variable)
- **Empathy/Apathy**: -0.8 to 0.2 (mostly selfish/hostile)
- **Health/Damage**: 0.2 to 0.6 (poor health condition)
- **Meta-Capability**: 0.0 to 0.1 (impaired judgment)

**Physical Properties:**
- **Max Health**: 25-45 HP (low due to drug use)
- **Movement Speed**: 2.5-6.0 m/s (highly variable)
- **Detection Range**: 5-20 meters (paranoia/alertness swings)
- **Memory Duration**: 10-180 seconds (extremely inconsistent)

**Behavioral Patterns:**
- **Default State**: Erratic movement, talking to itself
- **Paranoid Episodes**: Attacks random targets
- **Crash Periods**: Nearly catatonic, vulnerable
- **High Periods**: Hyperactive, aggressive

**Faction**: None (operates solo)
**Spawn Weight**: 10% in urban areas, 15% in abandoned areas
**Special Abilities**:
- **Unpredictable**: Behavior changes every 15-30 seconds
- **Pain Resistance**: Takes 50% longer to be stunned
- **Paranoid Awareness**: Sometimes detects player through walls

**Behavioral State Machine:**
```
PARANOID (30s): High speed, attacks anything moving
↓
AGGRESSIVE (20s): Focused attacks, higher damage
↓  
CRASH (45s): Slow movement, low awareness
↓
RECOVERY (15s): Gradually returning to paranoid
```

---

## 7. KAREN
*Entitled middle-class enforcer of social order*

**Base Stats:**
- **Fear/Courage**: 0.1 to 0.6 (bold when backed by authority)
- **Empathy/Apathy**: -0.4 to 0.1 (judgmental, self-righteous)
- **Health/Damage**: 0.6 to 0.8 (comfortable lifestyle)
- **Meta-Capability**: 0.0 to 0.2 (no special abilities, but persistent)

**Physical Properties:**
- **Max Health**: 60-80 HP
- **Movement Speed**: 3.0-3.8 m/s
- **Detection Range**: 12-16 meters
- **Memory Duration**: 180+ seconds (holds grudges)

**Behavioral Patterns:**
- **Default State**: Inspecting area, judging other NPCs
- **Player Assessment**: Immediate disapproval of "abnormal" behavior
- **Function Usage Response**: "That's not allowed!" - calls authorities
- **Escalation**: Verbal harassment → Authority summoning → Direct confrontation

**Faction**: KAREN_COLLECTIVE (coordinates with other Karens)
**Spawn Weight**: 15% in suburban areas, 5% elsewhere
**Special Abilities**:
- **Authority Call**: Summons law enforcement (70% success rate)
- **Social Pressure**: Influences other civilians against player
- **Persistence**: Continues harassment much longer than other NPCs

**Social Weapons:**
- **Public Shaming**: Reduces player reputation with civilians
- **Authority Appeals**: Calls cops, FBI agents, or management
- **Collective Action**: Rallies other NPCs against "undesirable" behavior

---

# SPECIAL FACTION NPCS (5 Types)

## 8. FBI_AGENT
*Professional reality anomaly investigator*

**Base Stats:**
- **Fear/Courage**: 0.5 to 0.8 (professionally trained)
- **Empathy/Apathy**: -0.1 to 0.3 (duty-focused)
- **Health/Damage**: 0.8 to 1.0 (peak condition)
- **Meta-Capability**: 0.4 to 0.7 (advanced function detection/counter)

**Physical Properties:**
- **Max Health**: 120-160 HP
- **Movement Speed**: 4.5-5.2 m/s
- **Detection Range**: 25-35 meters
- **Memory Duration**: Permanent (shared with other agents)

**Equipment:**
- **Weapon**: Advanced energy weapon (40-60 damage)
- **Armor**: Heavy protection (-50% all damage types)
- **Scanner**: Detects function usage within 50 meters
- **Communication**: Instant network with all FBI agents

**Behavioral Patterns:**
- **Spawn Trigger**: Player uses functions >0.7 power level
- **Investigation Mode**: Non-hostile information gathering
- **Documentation**: Records player abilities and behavior
- **Intervention**: Only becomes hostile if player attacks first

**Faction**: FBI_AGENTS
**Spawn Weight**: Triggered spawn only
**Special Abilities**:
- **Function Scanner**: Detects all function usage in area
- **Reality Stabilizer**: Can temporarily increase local world stability
- **Network Intelligence**: Shares information with all other FBI agents
- **Non-Lethal Priority**: Attempts capture rather than killing

**Investigation Protocol:**
1. **Approach**: Professional, non-threatening
2. **Question**: "We need to talk about the anomalies"
3. **Document**: Records player responses and abilities
4. **Report**: Updates central database
5. **Monitor**: Continues observation unless threatened

---

## 9. TERRY
*Hostile glowing agent - corrupted FBI variant*

**Base Stats:**
- **Fear/Courage**: 0.7 to 1.0 (fearless hunter)
- **Empathy/Apathy**: -0.9 to -0.6 (actively malevolent)
- **Health/Damage**: 0.9 to 1.0 (enhanced condition)
- **Meta-Capability**: 0.6 to 0.9 (corrupted reality manipulation)

**Physical Properties:**
- **Max Health**: 180-240 HP
- **Movement Speed**: 5.0-6.5 m/s (enhanced)
- **Detection Range**: 40-60 meters (supernatural awareness)
- **Memory Duration**: Infinite (dimensional tracking)

**Visual Properties:**
- **Appearance**: Glowing outline, distorted features
- **Eyes**: Bright points of light
- **Movement**: Slightly phases in/out of reality
- **Voice**: Distorted, echoing

**Equipment:**
- **Weapon**: Reality-distortion weapon (60-90 damage, ignores armor)
- **Abilities**: Phase through walls, temporary invisibility
- **Tracking**: Can follow player across dimensional boundaries

**Behavioral Patterns:**
- **Spawn Trigger**: Player kills FBI agents OR uses reality-breaking functions
- **Hunt Mode**: Relentless pursuit of player
- **Pack Tactics**: Coordinates with other Terries for pincer attacks
- **Reality Manipulation**: Uses corrupted versions of player functions

**Faction**: TERRIES
**Spawn Weight**: Triggered by hostile actions against FBI
**Special Abilities**:
- **Phase Walk**: Can move through solid matter
- **Reality Distortion**: Attacks ignore physical defenses
- **Dimensional Tracking**: Follows player through portals
- **Corruption Field**: Destabilizes world around them

**Combat Pattern:**
1. **Approach**: Phase through obstacles for direct path
2. **Engage**: Reality-distortion attacks
3. **Coordinate**: Flanking maneuvers with other Terries
4. **Persist**: Never gives up hunt, tracks across levels

---

## 10. LIZARD_MAN
*Shapeshifting infiltrator posing as civilian*

**Base Stats (True Form):**
- **Fear/Courage**: 0.4 to 0.8 (predator confidence)
- **Empathy/Apathy**: -0.7 to -0.3 (alien indifference)
- **Health/Damage**: 0.8 to 1.0 (superior physiology)
- **Meta-Capability**: 0.3 to 0.6 (advanced mimicry abilities)

**Base Stats (Disguised):**
- **Mimics target NPC type exactly**
- **Slight tells**: Perfect behavior (uncanny valley effect)

**Physical Properties (True Form):**
- **Max Health**: 140-180 HP
- **Movement Speed**: 5.5-6.8 m/s
- **Detection Range**: 30-45 meters (predator senses)
- **Memory Duration**: Permanent (collective memory)

**Disguise Properties:**
- **Perfect Mimicry**: Copies appearance and basic behavior
- **Behavioral Tells**: Too perfect, no random actions
- **Stress Reveals**: Disguise slips under pressure
- **Group Knowledge**: Shares mimicry data with other lizard people

**Behavioral Patterns:**
- **Default State**: Perfect imitation of target NPC type
- **Player Observation**: Subtle stalking and assessment
- **Opportunity Assessment**: Waits for player vulnerability
- **Strike Timing**: Attacks when player is distracted/weakened

**Faction**: LIZARD_MEN
**Spawn Weight**: 5% (randomly replaces other NPCs)
**Special Abilities**:
- **Perfect Disguise**: Undetectable until revealed
- **Ambush Predator**: +100% damage on surprise attacks
- **Collective Intelligence**: Shares information with other lizard people
- **Revelation Trigger**: Combat form 3x stronger than disguised form

**Detection Methods (Player Can Learn):**
- **Too Perfect**: Never makes mistakes other NPCs would
- **No Random Behavior**: Lacks the small behavioral quirks
- **Stress Testing**: Unusual situations cause disguise slips
- **Function Scanning**: High-level functions can detect true nature

---

## 11. HOMELESS_TRIBE_LEADER
*Charismatic survivor organizing mutual aid*

**Base Stats:**
- **Fear/Courage**: 0.2 to 0.7 (protective of tribe)
- **Empathy/Apathy**: 0.5 to 0.9 (community-focused)
- **Health/Damage**: 0.4 to 0.8 (survival-hardened)
- **Meta-Capability**: 0.1 to 0.4 (learned skills through necessity)

**Physical Properties:**
- **Max Health**: 80-120 HP
- **Movement Speed**: 3.8-4.5 m/s
- **Detection Range**: 20-30 meters (leadership awareness)
- **Memory Duration**: Permanent (community knowledge)

**Leadership Abilities:**
- **Rally**: Can organize 3-8 homeless NPCs for collective action
- **Resource Sharing**: Distributes found items among tribe
- **Safe Haven**: Creates temporary protected areas for tribe
- **Negotiation**: Can establish trade relationships with player

**Behavioral Patterns:**
- **Default State**: Coordinating tribe activities, resource management
- **Player Assessment**: Judges based on treatment of homeless NPCs
- **Protective**: Will fight to defend tribe members
- **Diplomatic**: Prefers negotiation to violence

**Faction**: HOMELESS_TRIBE (leader)
**Spawn Weight**: 1 per homeless group (3-8 members)
**Special Abilities**:
- **Inspire**: Nearby homeless NPCs gain +25% courage
- **Coordinate**: Can direct complex group actions
- **Trading Network**: Has access to hidden resources and information

**Relationship Dynamics:**
- **Help tribe members**: +5 reputation
- **Harm tribe members**: -10 reputation, possible tribal war
- **Trade fairly**: Gradual reputation increase
- **Share resources**: Major reputation boost

---

## 12. KAREN_COLLECTIVE_MANAGER
*Peak Karen with institutional authority*

**Base Stats:**
- **Fear/Courage**: 0.4 to 0.8 (authority-backed confidence)
- **Empathy/Apathy**: -0.6 to -0.2 (institutionally cruel)
- **Health/Damage**: 0.7 to 0.9 (comfortable lifestyle)
- **Meta-Capability**: 0.1 to 0.3 (bureaucratic powers)

**Physical Properties:**
- **Max Health**: 90-130 HP
- **Movement Speed**: 3.5-4.2 m/s
- **Detection Range**: 15-25 meters
- **Memory Duration**: Permanent (keeps detailed records)

**Authority Powers:**
- **Summon Security**: Calls professional security forces
- **Policy Enforcement**: Can temporarily restrict player actions in area
- **Social Network**: Coordinates with other authority figures
- **Documentation**: Creates permanent records affecting player reputation

**Behavioral Patterns:**
- **Default State**: Inspecting area for "violations"
- **Rule Enforcement**: Aggressive application of arbitrary standards
- **Escalation Management**: Methodical escalation through authority hierarchy
- **Public Relations**: Maintains facade of reasonableness

**Faction**: KAREN_COLLECTIVE (leader)
**Spawn Weight**: 1 per Karen group, or in "managed" areas
**Special Abilities**:
- **Authority Network**: Can summon various types of enforcement
- **Policy Creation**: Establishes temporary "rules" other NPCs follow
- **Social Manipulation**: Turns other NPCs against rule violators
- **Institutional Memory**: Player actions have lasting consequences

**Escalation Hierarchy:**
1. **Verbal Warning**: "That's against policy"
2. **Documentation**: Records violation for future reference
3. **Authority Escalation**: Calls appropriate enforcement
4. **Social Pressure**: Organizes community against player
5. **Institutional Response**: Permanent area restrictions/hostility

---

# FACTION RELATIONSHIPS

## Cooperation Matrix
```
              CIV  FBI  TER  LIZ  HOM  KAR
CIVILIAN      +++   +    -    0   ++   ++
FBI_AGENTS     +   +++  ---   --   0    +
TERRIES        -   ---  +++   0    -    -
LIZARD_MEN     0    --   0   +++   -    0
HOMELESS_TRIBE ++    0    -    -   +++   -
KAREN_COLLECTIVE ++   +    -    0    -   +++
```

**Legend**: +++ Alliance | ++ Cooperation | + Friendly | 0 Neutral | - Suspicious | -- Hostile | --- Warfare

## Dynamic Relationships

**Player Actions Affect Inter-Faction Relations:**
- **Help civilians**: FBI agents become more cooperative
- **Use high-level functions**: Spawns FBI investigation
- **Kill FBI agents**: Spawns Terries, civilians become fearful
- **Help homeless**: Karen Collective becomes more hostile
- **Excessive reality manipulation**: All factions become concerned

**Emergent Faction Dynamics:**
- **FBI-Civilian Cooperation**: Increased when player is violent
- **Homeless-Civilian Alliance**: Forms against Karen harassment
- **Multi-Faction Response**: Extreme player actions unite normally opposing factions

This NPC system creates a living social ecosystem where player actions have cascading consequences across multiple interconnected communities.
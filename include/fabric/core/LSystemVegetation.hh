#pragma once

#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace fabric {

/// Rule set defining an L-system grammar and turtle interpretation parameters.
struct LSystemRule {
    std::string axiom;                 ///< Initial string (e.g. "F" or "X").
    std::map<char, std::string> rules; ///< Production rules: char -> replacement.
    int iterations = 3;                ///< Number of rewriting iterations.
    float angle = 25.0f;               ///< Default turning angle in degrees.
    float segmentLength = 1.0f;        ///< Length of each 'F' step.
    float radiusDecay = 0.7f;          ///< Multiplicative radius decay per '[' push.
};

/// A line segment produced by turtle interpretation.
struct TurtleSegment {
    glm::vec3 start{0.0f}; ///< Segment start position.
    glm::vec3 end{0.0f};   ///< Segment end position.
    float radius = 1.0f;   ///< Segment radius (decays along branches).
    int materialTag = 0;   ///< 0 = wood, 1 = leaf.
};

// ---- Built-in presets ----

/// Bushy shrub: short segments, wide branching angle, 3 iterations.
inline const LSystemRule kBushRule = {
    "F",                             // axiom
    {{'F', "FF+[+F-F-F]-[-F+F+F]"}}, // rules
    3,                               // iterations
    25.0f,                           // angle (degrees)
    0.5f,                            // segmentLength
    0.75f                            // radiusDecay
};

/// Small deciduous tree: medium segments, moderate angle, 4 iterations.
inline const LSystemRule kSmallTreeRule = {
    "X",                                 // axiom
    {{'X', "F[+X][-X]FX"}, {'F', "FF"}}, // rules
    4,                                   // iterations
    22.0f,                               // angle (degrees)
    1.0f,                                // segmentLength
    0.7f                                 // radiusDecay
};

/// Large tree: longer segments, narrow angle, 5 iterations.
inline const LSystemRule kLargeTreeRule = {
    "X",                                  // axiom
    {{'X', "F[+X]F[-X]+X"}, {'F', "FF"}}, // rules
    5,                                    // iterations
    20.0f,                                // angle (degrees)
    2.0f,                                 // segmentLength
    0.65f                                 // radiusDecay
};

/// Expand an L-system grammar by applying production rules for N iterations.
/// Returns the fully expanded string.
std::string expand(const LSystemRule& rule);

/// Interpret an expanded L-system string using a 3D turtle.
/// Produces line segments with position, radius, and material tags.
///
/// Turtle commands:
///   F  - move forward, creating a segment
///   f  - move forward without creating a segment
///   +  - yaw left (rotate around up axis by +angle)
///   -  - yaw right (rotate around up axis by -angle)
///   ^  - pitch up (rotate around right axis by +angle)
///   &  - pitch down (rotate around right axis by -angle)
///   \  - roll left (rotate around forward axis by +angle)
///   /  - roll right (rotate around forward axis by -angle)
///   [  - push turtle state (position, orientation, radius)
///   ]  - pop turtle state
///   L  - switch material tag to "leaf" (1)
std::vector<TurtleSegment> interpret(const std::string& expanded, const LSystemRule& params);

} // namespace fabric

#pragma once

#include "fabric/input/InputSource.hh"
#include <cstdint>
#include <SDL3/SDL_keycode.h>
#include <string>
#include <vector>

namespace fabric {

/// Response curve applied after dead zone filtering
enum class ResponseCurve : uint8_t {
    Linear,
    Quadratic,
    Cubic
};

/// A key pair that maps two keys to a [-1, +1] axis.
/// Negative key maps to -1, positive key maps to +1.
/// When both are held, they cancel to 0.
struct KeyPairSource {
    SDL_Keycode negative = SDLK_UNKNOWN;
    SDL_Keycode positive = SDLK_UNKNOWN;
    bool operator==(const KeyPairSource&) const = default;
};

/// A single contributor to an axis value. Either a raw InputSource
/// (gamepad axis, mouse delta) or a KeyPairSource (digital-to-analog).
struct AxisSource {
    /// Raw analog source (gamepad axis, mouse delta, etc.)
    InputSource source;

    /// Key pair for digital-to-analog conversion.
    /// Active when useKeyPair is true; source is ignored in that case.
    KeyPairSource keyPair = {};

    /// If true, use keyPair instead of source
    bool useKeyPair = false;

    /// Per-source scale multiplier (applied before dead zone)
    float scale = 1.0f;
};

/// Binding from an axis name to one or more analog input sources.
/// Multiple sources contribute via priority: the first non-zero source wins.
struct AxisBinding {
    std::string name;
    std::vector<AxisSource> sources;

    /// Dead zone: values with absolute magnitude below this are clamped to 0.
    /// Applied after scale, before response curve.
    float deadZone = 0.0f;

    /// Response curve applied after dead zone remapping
    ResponseCurve responseCurve = ResponseCurve::Linear;

    /// Invert the final value (multiply by -1)
    bool inverted = false;

    /// Clamp range. Default [-1, 1]. Use [0, 1] for triggers.
    float rangeMin = -1.0f;
    float rangeMax = 1.0f;
};

/// Apply dead zone remapping: values below dead zone become 0,
/// values above are rescaled to fill the remaining range.
float applyDeadZone(float rawValue, float deadZone);

/// Apply response curve to a [-1, 1] value
float applyResponseCurve(float value, ResponseCurve curve);

/// Full axis processing pipeline: dead zone -> curve -> invert -> clamp
float processAxisValue(float rawValue, const AxisBinding& binding);

} // namespace fabric

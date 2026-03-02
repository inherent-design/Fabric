#include "fabric/core/InputAxis.hh"
#include <algorithm>
#include <cmath>

namespace fabric {

float applyDeadZone(float rawValue, float deadZone) {
    if (deadZone <= 0.0f)
        return rawValue;
    if (deadZone >= 1.0f)
        return 0.0f;

    float absVal = std::abs(rawValue);
    if (absVal < deadZone)
        return 0.0f;

    // Remap [deadZone, 1.0] to [0.0, 1.0]
    float sign = rawValue > 0.0f ? 1.0f : -1.0f;
    return sign * (absVal - deadZone) / (1.0f - deadZone);
}

float applyResponseCurve(float value, ResponseCurve curve) {
    switch (curve) {
        case ResponseCurve::Linear:
            return value;
        case ResponseCurve::Quadratic: {
            float sign = value >= 0.0f ? 1.0f : -1.0f;
            return sign * value * value;
        }
        case ResponseCurve::Cubic:
            // value^3 preserves sign naturally
            return value * value * value;
    }
    return value;
}

float processAxisValue(float rawValue, const AxisBinding& binding) {
    float value = applyDeadZone(rawValue, binding.deadZone);
    value = applyResponseCurve(value, binding.responseCurve);
    if (binding.inverted)
        value = -value;
    return std::clamp(value, binding.rangeMin, binding.rangeMax);
}

} // namespace fabric

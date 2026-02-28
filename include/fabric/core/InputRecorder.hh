#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fabric {

/// Event types for input recording (maps to SDL event type categories)
enum class InputEventType : uint32_t {
    KEY_DOWN = 0,
    KEY_UP = 1,
    MOUSE_MOTION = 2,
    MOUSE_BUTTON_DOWN = 3,
    MOUSE_BUTTON_UP = 4,
    MOUSE_WHEEL = 5,
    TEXT_INPUT = 6
};

/// Modifier key bitmask values
enum InputModifier : uint16_t {
    MOD_NONE = 0x0000,
    MOD_SHIFT = 0x0001,
    MOD_CTRL = 0x0002,
    MOD_ALT = 0x0004,
    MOD_GUI = 0x0008
};

/// SDL-independent serializable input event.
/// Stores raw numeric values that map to SDL event types without depending on SDL headers.
struct SerializedEvent {
    uint32_t eventType = 0;  ///< Maps to InputEventType enum values
    int32_t keycode = 0;     ///< SDL keycode, 0 if not keyboard event
    int32_t mouseX = 0;      ///< Mouse position X
    int32_t mouseY = 0;      ///< Mouse position Y
    int32_t mouseDeltaX = 0; ///< Relative motion X
    int32_t mouseDeltaY = 0; ///< Relative motion Y
    uint8_t button = 0;      ///< Mouse button index, 0 if not mouse button event
    uint16_t modifiers = 0;  ///< Modifier key bitmask (InputModifier flags)
    std::string text;        ///< Text input, empty if not text event

    bool operator==(const SerializedEvent& other) const = default;
};

/// A single frame of recorded input
struct InputFrame {
    uint64_t frameNumber = 0;
    float deltaTime = 0.0f; ///< Frame duration in seconds
    std::vector<SerializedEvent> events;

    bool operator==(const InputFrame& other) const = default;
};

/// Recording metadata
struct InputRecordingMetadata {
    std::string version = "1.0";
    std::string description;
    uint64_t totalFrames = 0;
    float totalDuration = 0.0f;

    bool operator==(const InputRecordingMetadata& other) const = default;
};

/// A complete input recording: sequence of frames with metadata
struct InputRecording {
    std::vector<InputFrame> frames;
    InputRecordingMetadata metadata;

    /// Append a frame to the recording
    void addFrame(const InputFrame& frame);

    /// Sum of all frame deltaTimes
    float totalDuration() const;

    /// Number of recorded frames
    uint64_t frameCount() const;

    /// Reset to empty state
    void clear();

    bool operator==(const InputRecording& other) const = default;
};

// --- ADL JSON serialization (nlohmann convention) ---

void to_json(nlohmann::json& j, const SerializedEvent& e);
void from_json(const nlohmann::json& j, SerializedEvent& e);

void to_json(nlohmann::json& j, const InputFrame& f);
void from_json(const nlohmann::json& j, InputFrame& f);

void to_json(nlohmann::json& j, const InputRecordingMetadata& m);
void from_json(const nlohmann::json& j, InputRecordingMetadata& m);

void to_json(nlohmann::json& j, const InputRecording& r);
void from_json(const nlohmann::json& j, InputRecording& r);

} // namespace fabric

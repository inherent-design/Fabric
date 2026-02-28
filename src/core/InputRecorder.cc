#include "fabric/core/InputRecorder.hh"

namespace fabric {

// --- InputRecording convenience methods ---

void InputRecording::addFrame(const InputFrame& frame) {
    frames.push_back(frame);
}

float InputRecording::totalDuration() const {
    float total = 0.0f;
    for (const auto& frame : frames) {
        total += frame.deltaTime;
    }
    return total;
}

uint64_t InputRecording::frameCount() const {
    return static_cast<uint64_t>(frames.size());
}

void InputRecording::clear() {
    frames.clear();
    metadata = InputRecordingMetadata{};
}

// --- SerializedEvent JSON ---

void to_json(nlohmann::json& j, const SerializedEvent& e) {
    j = nlohmann::json{{"eventType", e.eventType}, {"keycode", e.keycode},         {"mouseX", e.mouseX},
                       {"mouseY", e.mouseY},       {"mouseDeltaX", e.mouseDeltaX}, {"mouseDeltaY", e.mouseDeltaY},
                       {"button", e.button},       {"modifiers", e.modifiers},     {"text", e.text}};
}

void from_json(const nlohmann::json& j, SerializedEvent& e) {
    e.eventType = j.value("eventType", static_cast<uint32_t>(0));
    e.keycode = j.value("keycode", static_cast<int32_t>(0));
    e.mouseX = j.value("mouseX", static_cast<int32_t>(0));
    e.mouseY = j.value("mouseY", static_cast<int32_t>(0));
    e.mouseDeltaX = j.value("mouseDeltaX", static_cast<int32_t>(0));
    e.mouseDeltaY = j.value("mouseDeltaY", static_cast<int32_t>(0));
    e.button = j.value("button", static_cast<uint8_t>(0));
    e.modifiers = j.value("modifiers", static_cast<uint16_t>(0));
    e.text = j.value("text", std::string{});
}

// --- InputFrame JSON ---

void to_json(nlohmann::json& j, const InputFrame& f) {
    j = nlohmann::json{{"frameNumber", f.frameNumber}, {"deltaTime", f.deltaTime}, {"events", f.events}};
}

void from_json(const nlohmann::json& j, InputFrame& f) {
    f.frameNumber = j.value("frameNumber", static_cast<uint64_t>(0));
    f.deltaTime = j.value("deltaTime", 0.0f);
    f.events = j.value("events", std::vector<SerializedEvent>{});
}

// --- InputRecordingMetadata JSON ---

void to_json(nlohmann::json& j, const InputRecordingMetadata& m) {
    j = nlohmann::json{{"version", m.version},
                       {"description", m.description},
                       {"totalFrames", m.totalFrames},
                       {"totalDuration", m.totalDuration}};
}

void from_json(const nlohmann::json& j, InputRecordingMetadata& m) {
    m.version = j.value("version", std::string{"1.0"});
    m.description = j.value("description", std::string{});
    m.totalFrames = j.value("totalFrames", static_cast<uint64_t>(0));
    m.totalDuration = j.value("totalDuration", 0.0f);
}

// --- InputRecording JSON ---

void to_json(nlohmann::json& j, const InputRecording& r) {
    j = nlohmann::json{{"metadata", r.metadata}, {"frames", r.frames}};
}

void from_json(const nlohmann::json& j, InputRecording& r) {
    if (j.contains("metadata")) {
        r.metadata = j["metadata"].get<InputRecordingMetadata>();
    } else {
        r.metadata = InputRecordingMetadata{};
    }
    r.frames = j.value("frames", std::vector<InputFrame>{});
}

} // namespace fabric

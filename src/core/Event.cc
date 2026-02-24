#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Utils.hh"
#include <algorithm>
#include <type_traits>

namespace fabric {

Event::Event(const std::string& type, const std::string& source) : type(type), source(source) {
    if (type.empty()) {
        throwError("Event type cannot be empty");
    }
}

const std::string& Event::getType() const {
    return type;
}

const std::string& Event::getSource() const {
    return source;
}

bool Event::hasData(const std::string& key) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    return data.find(key) != data.end();
}

template <typename T> void Event::setData(const std::string& key, const T& value) {
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, float> ||
                      std::is_same_v<T, double> || std::is_same_v<T, std::string> ||
                      std::is_same_v<T, std::vector<uint8_t>>,
                  "Data type not supported. Must be one of the types in DataValue.");

    std::lock_guard<std::mutex> lock(dataMutex);
    data[key] = value;
}

template <typename T> T Event::getData(const std::string& key) const {
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, float> ||
                      std::is_same_v<T, double> || std::is_same_v<T, std::string> ||
                      std::is_same_v<T, std::vector<uint8_t>>,
                  "Data type not supported. Must be one of the types in DataValue.");

    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = data.find(key);
    if (it == data.end()) {
        throwError("Event data key '" + key + "' not found");
    }

    try {
        return std::get<T>(it->second);
    } catch (const std::bad_variant_access&) {
        throwError("Event data key '" + key + "' has incorrect type");
        return T();
    }
}

bool Event::hasAnyData(const std::string& key) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    return anyData.find(key) != anyData.end();
}

bool Event::isHandled() const {
    return handled;
}

void Event::setHandled(bool handled) {
    this->handled = handled;
}

bool Event::isCancelled() const {
    return cancelled;
}

void Event::setCancelled(bool cancelled) {
    this->cancelled = cancelled;
}

// Explicit template instantiations
template void Event::setData<int>(const std::string&, const int&);
template void Event::setData<float>(const std::string&, const float&);
template void Event::setData<double>(const std::string&, const double&);
template void Event::setData<bool>(const std::string&, const bool&);
template void Event::setData<std::string>(const std::string&, const std::string&);
template void Event::setData<std::vector<uint8_t>>(const std::string&, const std::vector<uint8_t>&);

template int Event::getData<int>(const std::string&) const;
template float Event::getData<float>(const std::string&) const;
template double Event::getData<double>(const std::string&) const;
template bool Event::getData<bool>(const std::string&) const;
template std::string Event::getData<std::string>(const std::string&) const;
template std::vector<uint8_t> Event::getData<std::vector<uint8_t>>(const std::string&) const;

std::string EventDispatcher::addEventListener(const std::string& eventType, const EventHandler& handler,
                                              int32_t priority) {
    if (eventType.empty()) {
        throwError("Event type cannot be empty");
    }

    if (!handler) {
        throwError("Event handler cannot be null");
    }

    std::lock_guard<std::mutex> lock(listenersMutex);

    HandlerEntry entry;
    entry.id = Utils::generateUniqueId("h_");
    entry.handler = handler;
    entry.priority = priority;

    // Insert in priority-sorted order (lower priority first).
    // upper_bound preserves insertion order for equal priorities.
    auto& vec = listeners[eventType];
    auto pos = std::upper_bound(vec.begin(), vec.end(), entry,
                                [](const HandlerEntry& a, const HandlerEntry& b) { return a.priority < b.priority; });
    vec.insert(pos, entry);

    FABRIC_LOG_DEBUG("Added event listener for type '{}' with ID '{}' (priority {})", eventType, entry.id, priority);

    return entry.id;
}

bool EventDispatcher::removeEventListener(const std::string& eventType, const std::string& handlerId) {
    std::lock_guard<std::mutex> lock(listenersMutex);

    auto it = listeners.find(eventType);
    if (it == listeners.end()) {
        return false;
    }

    auto& handlers = it->second;
    auto handlerIt = std::find_if(handlers.begin(), handlers.end(),
                                  [&handlerId](const HandlerEntry& entry) { return entry.id == handlerId; });

    if (handlerIt != handlers.end()) {
        handlers.erase(handlerIt);
        FABRIC_LOG_DEBUG("Removed event listener for type '{}' with ID '{}'", eventType, handlerId);
        return true;
    }

    return false;
}

bool EventDispatcher::dispatchEvent(Event& event) {
    std::vector<HandlerEntry> handlersToInvoke;

    {
        std::lock_guard<std::mutex> lock(listenersMutex);

        auto it = listeners.find(event.getType());
        if (it == listeners.end()) {
            return false;
        }

        handlersToInvoke = it->second;
    }

    bool handled = false;

    for (const auto& entry : handlersToInvoke) {
        try {
            entry.handler(event);
            if (event.isCancelled() || event.isHandled()) {
                handled = true;
                break;
            }
        } catch (const std::exception& e) {
            FABRIC_LOG_ERROR("Exception in event handler: {}", e.what());
        } catch (...) {
            FABRIC_LOG_ERROR("Unknown exception in event handler");
        }
    }

    return handled;
}

} // namespace fabric

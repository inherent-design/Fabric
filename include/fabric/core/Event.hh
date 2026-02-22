#pragma once

#include "fabric/core/Types.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <any>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace fabric {

class Event {
public:
  using DataValue = Variant;

  Event(const std::string& type, const std::string& source);
  virtual ~Event() = default;

  const std::string& getType() const;
  const std::string& getSource() const;

  // Variant-typed data (original interface)
  template <typename T>
  void setData(const std::string& key, const T& value);

  template <typename T>
  T getData(const std::string& key) const;

  bool hasData(const std::string& key) const;

  // Any-typed data for richer payloads without expanding Variant
  template <typename T>
  void setAnyData(const std::string& key, T value) {
    std::lock_guard<std::mutex> lock(dataMutex);
    anyData[key] = std::any(std::move(value));
  }

  template <typename T>
  T getAnyData(const std::string& key) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    auto it = anyData.find(key);
    if (it == anyData.end()) {
      throwError("Event any-data key '" + key + "' not found");
    }
    try {
      return std::any_cast<T>(it->second);
    } catch (const std::bad_any_cast&) {
      throwError("Event any-data key '" + key + "' has incorrect type");
      return T{}; // unreachable
    }
  }

  bool hasAnyData(const std::string& key) const;

  bool isHandled() const;
  void setHandled(bool handled = true);

  bool isCancelled() const;
  void setCancelled(bool cancelled = true);

private:
  std::string type;
  std::string source;
  mutable std::mutex dataMutex;
  std::unordered_map<std::string, DataValue> data;
  std::unordered_map<std::string, std::any> anyData;
  std::atomic<bool> handled{false};
  std::atomic<bool> cancelled{false};
};

using EventHandler = std::function<void(Event&)>;

class EventDispatcher {
public:
  EventDispatcher() = default;

  // Subscribe with optional priority (lower runs first, default 0)
  std::string addEventListener(const std::string& eventType,
                               const EventHandler& handler,
                               int32_t priority = 0);

  bool removeEventListener(const std::string& eventType, const std::string& handlerId);

  bool dispatchEvent(Event& event);

private:
  struct HandlerEntry {
    std::string id;
    EventHandler handler;
    int32_t priority = 0;
  };

  mutable std::mutex listenersMutex;
  std::unordered_map<std::string, std::vector<HandlerEntry>> listeners;
};

} // namespace fabric
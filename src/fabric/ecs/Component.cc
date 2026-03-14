#include "fabric/ecs/Component.hh"
#include "fabric/log/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <type_traits>

namespace fabric {

Component::Component(const std::string& id) : id_(id) {
    if (id.empty()) {
        throwError("Component ID cannot be empty");
    }
}

const std::string& Component::getId() const {
    return id_;
}

bool Component::hasProperty(const std::string& name) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_.find(name) != properties_.end();
}

bool Component::removeProperty(const std::string& name) {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_.erase(name) > 0;
}

void Component::addChild(std::shared_ptr<Component> child) {
    if (!child) {
        throwError("Cannot add null child to component");
    }

    std::lock_guard<std::mutex> lock(childrenMutex_);

    // Check for duplicate IDs
    for (const auto& existingChild : children_) {
        if (existingChild->getId() == child->getId()) {
            throwError("Child component with ID '" + child->getId() + "' already exists");
        }
    }

    children_.push_back(child);
    FABRIC_LOG_DEBUG("Added child '{}' to component '{}'", child->getId(), id_);
}

bool Component::removeChild(const std::string& childId) {
    std::lock_guard<std::mutex> lock(childrenMutex_);

    auto it = std::find_if(children_.begin(), children_.end(),
                           [&childId](const auto& child) { return child->getId() == childId; });

    if (it != children_.end()) {
        children_.erase(it);
        FABRIC_LOG_DEBUG("Removed child '{}' from component '{}'", childId, id_);
        return true;
    }

    return false;
}

std::shared_ptr<Component> Component::getChild(const std::string& childId) const {
    std::lock_guard<std::mutex> lock(childrenMutex_);

    auto it = std::find_if(children_.begin(), children_.end(),
                           [&childId](const auto& child) { return child->getId() == childId; });

    if (it != children_.end()) {
        return *it;
    }

    return nullptr;
}

std::vector<std::shared_ptr<Component>> Component::getChildren() const {
    std::lock_guard<std::mutex> lock(childrenMutex_);
    return children_;
}

template <typename T> void Component::setProperty(const std::string& name, const T& value) {
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, float> ||
                      std::is_same_v<T, double> || std::is_same_v<T, std::string> ||
                      std::is_same_v<T, std::shared_ptr<Component>>,
                  "Property type not supported. Must be one of the types in PropertyValue.");

    std::lock_guard<std::mutex> lock(propertiesMutex_);
    properties_[name] = value;
}

template <typename T> T Component::getProperty(const std::string& name) const {
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, float> ||
                      std::is_same_v<T, double> || std::is_same_v<T, std::string> ||
                      std::is_same_v<T, std::shared_ptr<Component>>,
                  "Property type not supported. Must be one of the types in PropertyValue.");

    std::lock_guard<std::mutex> lock(propertiesMutex_);

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        throwError("Property '" + name + "' not found in component '" + id_ + "'");
    }

    try {
        return std::get<T>(it->second);
    } catch (const std::bad_variant_access&) {
        throwError("Property '" + name + "' has incorrect type");
        // This line is never reached due to throwError, but needed for compilation
        return T();
    }
}

// Explicit template instantiations for common types
template void Component::setProperty<int>(const std::string&, const int&);
template void Component::setProperty<float>(const std::string&, const float&);
template void Component::setProperty<double>(const std::string&, const double&);
template void Component::setProperty<bool>(const std::string&, const bool&);
template void Component::setProperty<std::string>(const std::string&, const std::string&);
template void Component::setProperty<std::shared_ptr<Component>>(const std::string&, const std::shared_ptr<Component>&);

template int Component::getProperty<int>(const std::string&) const;
template float Component::getProperty<float>(const std::string&) const;
template double Component::getProperty<double>(const std::string&) const;
template bool Component::getProperty<bool>(const std::string&) const;
template std::string Component::getProperty<std::string>(const std::string&) const;
template std::shared_ptr<Component> Component::getProperty<std::shared_ptr<Component>>(const std::string&) const;

} // namespace fabric

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace fabric {

/// Base component class. Provides lifecycle methods, property storage, and child management.
class Component {
  public:
    /**
     * @brief Supported property value types
     *
     * This variant defines all types that can be stored in component properties.
     * To add support for additional types, extend this variant definition.
     */
    using PropertyValue = std::variant<bool, int, float, double, std::string, std::shared_ptr<Component>>;

    /**
     * @brief Component constructor
     *
     * @param id Unique identifier for the component
     * @throws FabricException if id is empty
     */
    explicit Component(const std::string& id);

    virtual ~Component() = default;

    const std::string& getId() const;

    /**
     * @brief Initialize the component
     *
     * This method is called after the component is created but before
     * it is rendered for the first time. Use this method to perform any
     * initialization tasks.
     */
    virtual void initialize() = 0;

    /**
     * @brief Render the component
     *
     * This method is called when the component needs to be rendered.
     * It should return a string representation of the component.
     *
     * @return String representation of the component
     */
    virtual std::string render() = 0;

    /**
     * @brief Update the component
     *
     * This method is called when the component needs to be updated.
     * Override this method to implement custom update logic.
     *
     * @param deltaTime Time elapsed since the last update in seconds
     */
    virtual void update(float deltaTime) = 0;

    /**
     * @brief Clean up component resources
     *
     * This method is called before the component is destroyed.
     * Override this method to perform any cleanup tasks.
     */
    virtual void cleanup() = 0;

    /**
     * @brief Set a property value
     *
     * @tparam T Type of the property value (must be one of the types in PropertyValue)
     * @param name Property name
     * @param value Property value
     */
    template <typename T> void setProperty(const std::string& name, const T& value);

    /**
     * @brief Get a property value
     *
     * @tparam T Expected type of the property value
     * @param name Property name
     * @return Property value
     * @throws FabricException if property doesn't exist or is wrong type
     */
    template <typename T> T getProperty(const std::string& name) const;

    bool hasProperty(const std::string& name) const;

    bool removeProperty(const std::string& name);

    /**
     * @brief Add a child component
     *
     * @param child Child component to add
     * @throws FabricException if child is null or if a child with the same ID already exists
     */
    void addChild(std::shared_ptr<Component> child);

    bool removeChild(const std::string& childId);

    std::shared_ptr<Component> getChild(const std::string& childId) const;

    std::vector<std::shared_ptr<Component>> getChildren() const;

  private:
    std::string id;
    mutable std::mutex propertiesMutex; // Mutex for thread-safe property access
    std::unordered_map<std::string, PropertyValue> properties;

    mutable std::mutex childrenMutex; // Mutex for thread-safe children access
    std::vector<std::shared_ptr<Component>> children;
};

} // namespace fabric

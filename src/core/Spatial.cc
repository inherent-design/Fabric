#include "fabric/core/Spatial.hh"
#include "fabric/core/Log.hh"
#include <cmath>
#include <algorithm>

namespace fabric {

// SceneNode implementation
void SceneNode::updateSelf(float deltaTime) {
    // Base implementation does nothing
}

void SceneNode::update(float deltaTime) {
    // Update this node
    updateSelf(deltaTime);
    
    // Update children
    for (auto& child : children_) {
        child->update(deltaTime);
    }
}

SceneNode* SceneNode::addChild(std::unique_ptr<SceneNode> child) {
    if (child->parent_) {
        FABRIC_LOG_DEBUG("SceneGraph: reparenting '{}' from '{}' to '{}'",
                         child->getName(), child->parent_->getName(), name_);
        child->parent_->detachChild(child.get());
    }

    SceneNode* childPtr = child.get();
    child->parent_ = this;
    children_.push_back(std::move(child));
    FABRIC_LOG_DEBUG("SceneGraph: added child '{}' to '{}'", childPtr->getName(), name_);
    return childPtr;
}

SceneNode* SceneNode::createChild(const std::string& name) {
    auto child = std::make_unique<SceneNode>(name);
    return addChild(std::move(child));
}

std::unique_ptr<SceneNode> SceneNode::detachChild(SceneNode* child) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (it->get() == child) {
            FABRIC_LOG_DEBUG("SceneGraph: detached '{}' from '{}'", child->getName(), name_);
            std::unique_ptr<SceneNode> result = std::move(*it);
            children_.erase(it);
            result->parent_ = nullptr;
            return result;
        }
    }
    return nullptr;
}

std::unique_ptr<SceneNode> SceneNode::detachChild(const std::string& name) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if ((*it)->getName() == name) {
            std::unique_ptr<SceneNode> result = std::move(*it);
            children_.erase(it);
            result->parent_ = nullptr;
            return result;
        }
    }
    return nullptr;
}

SceneNode* SceneNode::findChild(const std::string& name) {
    if (name_ == name) {
        return this;
    }
    
    for (const auto& child : children_) {
        if (SceneNode* result = child->findChild(name)) {
            return result;
        }
    }
    
    return nullptr;
}

// Scene implementation
Scene::Scene() : root_(std::make_unique<SceneNode>("root")) {}

SceneNode* Scene::getRoot() const { 
    return root_.get(); 
}

SceneNode* Scene::findNode(const std::string& name) const {
    return root_->findChild(name);
}

void Scene::update(float deltaTime) {
    root_->update(deltaTime);
}

// Template instantiations for common types
template class Vector2<float, Space::World>;
template class Vector2<float, Space::Local>;
template class Vector3<float, Space::World>;
template class Vector3<float, Space::Local>;
template class Vector4<float, Space::World>;
template class Vector4<float, Space::Local>;
template class Matrix4x4<float>;
template class Transform<float>;

} // namespace fabric
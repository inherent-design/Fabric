#include "fabric/core/Resource.hh"
#include "fabric/core/Log.hh"

namespace fabric {

// Initialize static members
std::mutex ResourceFactory::mutex_;
std::unordered_map<std::string, std::function<std::shared_ptr<Resource>(const std::string&)>> ResourceFactory::factories_;

bool ResourceFactory::isTypeRegistered(const std::string& typeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return factories_.find(typeId) != factories_.end();
}

std::shared_ptr<Resource> ResourceFactory::create(const std::string& typeId, const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = factories_.find(typeId);
    if (it == factories_.end()) {
        FABRIC_LOG_ERROR("ResourceFactory: unknown type '{}' for resource '{}'", typeId, id);
        return nullptr;
    }
    FABRIC_LOG_DEBUG("ResourceFactory: created resource '{}' of type '{}'", id, typeId);
    return it->second(id);
}

} // namespace fabric
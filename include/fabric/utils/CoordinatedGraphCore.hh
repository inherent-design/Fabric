#pragma once

#include "fabric/utils/CoordinatedGraphTypes.hh"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stack>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fabric {

/**
 * @brief A thread-safe directed acyclic graph implementation with intentional locking
 *
 * This graph implementation is designed for concurrent access with node-level
 * locking for maximum parallelism, while providing awareness of different
 * lock types and their intentions. It enables high-priority graph operations
 * to coordinate with lower-priority node operations.
 *
 * The graph enforces acyclicity to serve as a proper DAG (Directed Acyclic Graph),
 * which is essential for preventing deadlocks in dependency management.
 * It implements resource lock management with deadlock prevention through lock
 * ordering based on the graph structure.
 *
 * Key Features:
 * - Enforced acyclicity with cycle detection on all edge additions
 * - Intent-based locking (read, write, structure modification)
 * - Lock hierarchy to prevent deadlocks (graph lock -> node locks, never reverse)
 * - Two-phase locking protocol for advanced concurrency control
 * - Non-blocking operations with try_lock to avoid indefinite waits
 * - Thread-safe node access through explicit locking mechanisms
 * - Awareness propagation between locks of different levels
 * - Lock resource management with deadlock prevention
 *
 * @tparam T Type of data stored in graph nodes
 * @tparam KeyType Type used as node identifier (default: std::string)
 */
template <typename T, typename KeyType = std::string> class CoordinatedGraph {
  public:
    /**
     * @brief Lock intent type to specify the purpose of a lock
     */
    enum class LockIntent : std::uint8_t {
        Read,           // Intent to read without modification
        NodeModify,     // Intent to modify node data only
        GraphStructure, // Intent to modify graph structure (highest priority)
    };

    /**
     * @brief Status of a lock for notification callbacks
     */
    enum class LockStatus : std::uint8_t {
        Acquired,       // Lock has been acquired
        Released,       // Lock has been released
        Preempted,      // Lock has been preempted by higher priority
        BackgroundWait, // Lock is temporarily waiting for structural changes
        Failed          // Lock acquisition failed
    };

    /**
     * @brief Lock acquisition mode for resource locks
     */
    enum class LockMode : std::uint8_t {
        Shared,    // Multiple readers allowed
        Exclusive, // Single writer, no readers
        Upgrade,   // Initially shared, can be upgraded to exclusive
    };

    /**
     * @brief Status of a resource lock
     */
    enum class ResourceLockStatus : std::uint8_t {
        Unlocked,  // Not locked
        Shared,    // Held in shared mode
        Exclusive, // Held in exclusive mode
        Intention, // Held in intention mode
        Pending    // Lock acquisition in progress
    };

    /**
     * @brief Node states used for traversal algorithms
     */
    enum class NodeState : std::uint8_t {
        Unvisited,
        Visiting,
        Visited
    };

    // Forward declaration of lock handles
    class NodeLockHandle;
    class GraphLockHandle;
    class ResourceLockHandle;

    // -----------------------------------------------------------------------
    // Node
    // -----------------------------------------------------------------------

    /**
     * @brief A node in the graph
     *
     * Each node has its own lock for fine-grained concurrency control.
     */
    class Node {
      public:
        using LockCallback = std::function<void(LockStatus)>;

        Node(KeyType key, T data)
            : key_(std::move(key)), data_(std::move(data)), lastAccessTime_(std::chrono::steady_clock::now()) {}

        const KeyType& getKey() const { return key_; }

        /**
         * @brief Get the node's data (const version)
         *
         * WARNING: The returned reference outlives the internal lock. Caller
         * must hold an external lock (via tryLock / NodeLockHandle) for the
         * reference to be safe. Prefer getDataNoLock() with an explicit lock.
         */
        const T& getData() const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return data_;
        }

        /**
         * @brief Get the node's data (mutable version)
         *
         * WARNING: The returned reference outlives the internal lock. Caller
         * must hold an external lock (via tryLock / NodeLockHandle) for the
         * reference to be safe. Prefer getDataNoLock() with an explicit lock.
         * Will deadlock if called while a NodeLockHandle already holds this
         * node's mutex (use getDataNoLock instead).
         */
        T& getData() {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            lastAccessTime_ = std::chrono::steady_clock::now();
            return data_;
        }

        const T& getDataNoLock() const { return data_; }

        T& getDataNoLock() {
            lastAccessTime_ = std::chrono::steady_clock::now();
            return data_;
        }

        void setData(T data) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            data_ = std::move(data);
            lastAccessTime_ = std::chrono::steady_clock::now();
        }

        std::chrono::steady_clock::time_point getLastAccessTime() const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return lastAccessTime_;
        }

        void touch() {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            lastAccessTime_ = std::chrono::steady_clock::now();
        }

        std::unique_ptr<NodeLockHandle> tryLock(LockIntent intent, size_t timeoutMs = 100,
                                                LockCallback callback = nullptr) {
            using namespace std::chrono;

            if (intent == LockIntent::Read) {
                std::shared_lock<std::shared_mutex> lock(mutex_, std::try_to_lock);
                if (lock.owns_lock()) {
                    return std::make_unique<NodeLockHandle>(this, std::move(lock), intent, callback);
                }

                auto start = steady_clock::now();
                while (true) {
                    lock = std::shared_lock<std::shared_mutex>(mutex_, std::try_to_lock);
                    if (lock.owns_lock()) {
                        return std::make_unique<NodeLockHandle>(this, std::move(lock), intent, callback);
                    }

                    if (duration_cast<milliseconds>(steady_clock::now() - start).count() >= timeoutMs) {
                        return nullptr;
                    }

                    std::this_thread::sleep_for(milliseconds(1));
                }
            } else {
                std::unique_lock<std::shared_mutex> lock(mutex_, std::try_to_lock);
                if (lock.owns_lock()) {
                    return std::make_unique<NodeLockHandle>(this, std::move(lock), intent, callback);
                }

                auto start = steady_clock::now();
                while (true) {
                    lock = std::unique_lock<std::shared_mutex>(mutex_, std::try_to_lock);
                    if (lock.owns_lock()) {
                        return std::make_unique<NodeLockHandle>(this, std::move(lock), intent, callback);
                    }

                    if (duration_cast<milliseconds>(steady_clock::now() - start).count() >= timeoutMs) {
                        return nullptr;
                    }

                    std::this_thread::sleep_for(milliseconds(1));
                }
            }
        }

      private:
        friend class CoordinatedGraph;
        friend class NodeLockHandle;

        KeyType key_;
        T data_;
        std::chrono::steady_clock::time_point lastAccessTime_;
        mutable std::shared_mutex mutex_;
        std::vector<std::pair<LockIntent, LockCallback>> activeCallbacks_;
        std::mutex callbackMutex_;

        void notifyLockHolders(LockStatus status) {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            for (auto& [intent, callback] : activeCallbacks_) {
                if (callback) {
                    callback(status);
                }
            }
        }

        void registerCallback(LockIntent intent, LockCallback callback) {
            if (!callback)
                return;
            std::lock_guard<std::mutex> lock(callbackMutex_);
            activeCallbacks_.push_back({intent, callback});
        }

        void removeCallback(LockIntent intent, LockCallback callback) {
            if (!callback)
                return;
            std::lock_guard<std::mutex> lock(callbackMutex_);
            auto it = activeCallbacks_.begin();
            while (it != activeCallbacks_.end()) {
                if (it->first == intent) {
                    it = activeCallbacks_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    };

    // -----------------------------------------------------------------------
    // NodeLockHandle
    // -----------------------------------------------------------------------

    class NodeLockHandle {
      public:
        NodeLockHandle(Node* node, std::shared_lock<std::shared_mutex> lock, LockIntent intent,
                       typename Node::LockCallback callback)
            : node_(node),
              readLock_(std::move(lock)),
              writeLock_(),
              isReadLock_(true),
              intent_(intent),
              callback_(callback) {
            if (node_ && callback_) {
                node_->registerCallback(intent_, callback_);
            }
        }

        NodeLockHandle(Node* node, std::unique_lock<std::shared_mutex> lock, LockIntent intent,
                       typename Node::LockCallback callback)
            : node_(node),
              readLock_(),
              writeLock_(std::move(lock)),
              isReadLock_(false),
              intent_(intent),
              callback_(callback) {
            if (node_ && callback_) {
                node_->registerCallback(intent_, callback_);
            }
        }

        ~NodeLockHandle() {
            if (node_ && callback_) {
                node_->removeCallback(intent_, callback_);
            }
        }

        bool isLocked() const { return isReadLock_ ? readLock_.owns_lock() : writeLock_.owns_lock(); }

        void release() {
            if (isReadLock_) {
                readLock_.unlock();
            } else {
                writeLock_.unlock();
            }

            if (node_ && callback_) {
                node_->removeCallback(intent_, callback_);
                callback_ = nullptr;
            }
        }

        Node* getNode() const { return node_; }
        LockIntent getIntent() const { return intent_; }

      private:
        Node* node_;
        std::shared_lock<std::shared_mutex> readLock_;
        std::unique_lock<std::shared_mutex> writeLock_;
        bool isReadLock_;
        LockIntent intent_;
        typename Node::LockCallback callback_;
    };

    // -----------------------------------------------------------------------
    // GraphLockHandle
    // -----------------------------------------------------------------------

    class GraphLockHandle {
      public:
        GraphLockHandle(CoordinatedGraph* graph, std::shared_lock<std::shared_mutex> lock, LockIntent intent)
            : graph_(graph), readLock_(std::move(lock)), writeLock_(), isReadLock_(true), intent_(intent) {}

        GraphLockHandle(CoordinatedGraph* graph, std::unique_lock<std::shared_mutex> lock, LockIntent intent)
            : graph_(graph), readLock_(), writeLock_(std::move(lock)), isReadLock_(false), intent_(intent) {}

        ~GraphLockHandle() { release(); }

        bool isLocked() const { return isReadLock_ ? readLock_.owns_lock() : writeLock_.owns_lock(); }

        void release() {
            if (isReadLock_) {
                if (readLock_.owns_lock()) {
                    readLock_.unlock();
                    if (graph_) {
                        graph_->onGraphLockReleased(intent_);
                    }
                }
            } else {
                if (writeLock_.owns_lock()) {
                    writeLock_.unlock();
                    if (graph_) {
                        graph_->onGraphLockReleased(intent_);
                    }
                }
            }
        }

        LockIntent getIntent() const { return intent_; }

      private:
        CoordinatedGraph* graph_;
        std::shared_lock<std::shared_mutex> readLock_;
        std::unique_lock<std::shared_mutex> writeLock_;
        bool isReadLock_;
        LockIntent intent_;
    };

    // -----------------------------------------------------------------------
    // ResourceLockHandle
    // -----------------------------------------------------------------------

    /**
     * @brief A handle for a resource lock that automatically releases on destruction
     *
     * This class represents a high-level lock on a resource, which is backed by
     * the underlying node locking mechanism but with additional deadlock prevention
     * through the DAG ordering.
     */
    class ResourceLockHandle {
      public:
        ResourceLockHandle(CoordinatedGraph* graph, KeyType resourceKey, LockMode mode, ResourceLockStatus status,
                           std::thread::id ownerId)
            : graph_(graph),
              resourceKey_(std::move(resourceKey)),
              mode_(mode),
              status_(status),
              ownerId_(ownerId),
              isValid_(true) {}

        ~ResourceLockHandle() { release(); }

        void release() {
            if (isValid_ && status_ != ResourceLockStatus::Unlocked) {
                if (graph_) {
                    graph_->releaseResourceLock(resourceKey_, mode_, ownerId_);
                }
                status_ = ResourceLockStatus::Unlocked;
                isValid_ = false;
            }
        }

        bool upgrade(size_t timeoutMs = 100) {
            if (!isValid_ || mode_ != LockMode::Upgrade || status_ != ResourceLockStatus::Shared) {
                return false;
            }

            if (graph_) {
                bool success = graph_->upgradeResourceLock(resourceKey_, ownerId_, timeoutMs);
                if (success) {
                    status_ = ResourceLockStatus::Exclusive;
                }
                return success;
            }
            return false;
        }

        bool isLocked() const { return isValid_ && status_ != ResourceLockStatus::Unlocked; }
        ResourceLockStatus getStatus() const { return status_; }
        LockMode getMode() const { return mode_; }
        const KeyType& getResourceKey() const { return resourceKey_; }

      private:
        CoordinatedGraph* graph_;
        KeyType resourceKey_;
        LockMode mode_;
        ResourceLockStatus status_;
        std::thread::id ownerId_;
        bool isValid_;
    };

    // -----------------------------------------------------------------------
    // Core graph operations
    // -----------------------------------------------------------------------

    CoordinatedGraph() = default;
    ~CoordinatedGraph() = default;

    bool addNode(const KeyType& key, T data) {
        auto lock = lockGraph(LockIntent::GraphStructure);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for node addition");
        }

        if (nodes_.find(key) != nodes_.end()) {
            return false;
        }

        auto node = std::make_shared<Node>(key, std::move(data));
        nodes_[key] = node;
        outEdges_[key] = std::unordered_set<KeyType>();
        inEdges_[key] = std::unordered_set<KeyType>();

        return true;
    }

    bool removeNode(const KeyType& key) {
        auto lock = lockGraph(LockIntent::GraphStructure);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for node removal");
        }

        if (nodes_.find(key) == nodes_.end()) {
            return false;
        }

        auto nodePtr = nodes_[key];
        if (nodePtr) {
            nodePtr->notifyLockHolders(LockStatus::Preempted);
        }

        for (const auto& target : outEdges_[key]) {
            inEdges_[target].erase(key);
        }

        for (const auto& source : inEdges_[key]) {
            outEdges_[source].erase(key);
        }

        nodes_.erase(key);
        outEdges_.erase(key);
        inEdges_.erase(key);

        onNodeRemoved(key);

        return true;
    }

    bool hasNode(const KeyType& key) const {
        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for node check");
        }

        return nodes_.find(key) != nodes_.end();
    }

    std::shared_ptr<Node> getNode(const KeyType& key, size_t timeoutMs = 100) const {
        auto lock = lockGraph(LockIntent::Read, timeoutMs);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for getting node");
        }

        auto it = nodes_.find(key);
        return (it != nodes_.end()) ? it->second : nullptr;
    }

    std::unique_ptr<NodeLockHandle> tryLockNode(const KeyType& key, LockIntent intent, bool forWrite = false,
                                                size_t timeoutMs = 100,
                                                typename Node::LockCallback callback = nullptr) const {
        if (!canProceedWithIntent(intent)) {
            return nullptr;
        }

        auto graphLock = lockGraph(LockIntent::Read, timeoutMs);
        if (!graphLock || !graphLock->isLocked()) {
            return nullptr;
        }

        auto it = nodes_.find(key);
        if (it == nodes_.end()) {
            return nullptr;
        }

        auto node = it->second;

        graphLock->release();

        return node->tryLock(intent, timeoutMs, callback);
    }

    std::unique_ptr<GraphLockHandle> lockGraph(LockIntent intent, size_t timeoutMs = 100) const {
        using namespace std::chrono;

        if (intent == LockIntent::Read) {
            std::shared_lock<std::shared_mutex> lock(graphMutex_, std::try_to_lock);
            if (lock.owns_lock()) {
                return std::make_unique<GraphLockHandle>(const_cast<CoordinatedGraph*>(this), std::move(lock), intent);
            }

            auto start = steady_clock::now();
            while (true) {
                lock = std::shared_lock<std::shared_mutex>(graphMutex_, std::try_to_lock);
                if (lock.owns_lock()) {
                    return std::make_unique<GraphLockHandle>(const_cast<CoordinatedGraph*>(this), std::move(lock),
                                                             intent);
                }

                if (duration_cast<milliseconds>(steady_clock::now() - start).count() >= timeoutMs) {
                    return nullptr;
                }

                std::this_thread::sleep_for(milliseconds(1));
            }
        } else {
            if (intent == LockIntent::GraphStructure) {
                const_cast<CoordinatedGraph*>(this)->notifyAllNodeLockHolders(LockStatus::BackgroundWait);
            }

            std::unique_lock<std::shared_mutex> lock(graphMutex_, std::try_to_lock);
            if (lock.owns_lock()) {
                if (intent == LockIntent::GraphStructure) {
                    const_cast<CoordinatedGraph*>(this)->currentStructuralIntent_.store(static_cast<int>(intent),
                                                                                        std::memory_order_release);
                }

                return std::make_unique<GraphLockHandle>(const_cast<CoordinatedGraph*>(this), std::move(lock), intent);
            }

            auto start = steady_clock::now();
            while (true) {
                lock = std::unique_lock<std::shared_mutex>(graphMutex_, std::try_to_lock);
                if (lock.owns_lock()) {
                    if (intent == LockIntent::GraphStructure) {
                        const_cast<CoordinatedGraph*>(this)->currentStructuralIntent_.store(static_cast<int>(intent),
                                                                                            std::memory_order_release);
                    }

                    return std::make_unique<GraphLockHandle>(const_cast<CoordinatedGraph*>(this), std::move(lock),
                                                             intent);
                }

                if (duration_cast<milliseconds>(steady_clock::now() - start).count() >= timeoutMs) {
                    if (intent == LockIntent::GraphStructure) {
                        const_cast<CoordinatedGraph*>(this)->notifyAllNodeLockHolders(LockStatus::Acquired);
                    }

                    return nullptr;
                }

                std::this_thread::sleep_for(milliseconds(1));
            }
        }
    }

    // -----------------------------------------------------------------------
    // Edge operations
    // -----------------------------------------------------------------------

    bool addEdge(const KeyType& fromKey, const KeyType& toKey, bool detectCycles = true) {
        auto lock = lockGraph(LockIntent::GraphStructure);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for edge addition");
        }

        if (nodes_.find(fromKey) == nodes_.end() || nodes_.find(toKey) == nodes_.end()) {
            return false;
        }

        if (outEdges_[fromKey].find(toKey) != outEdges_[fromKey].end()) {
            return false;
        }

        outEdges_[fromKey].insert(toKey);
        inEdges_[toKey].insert(fromKey);

        bool hasCycleResult = false;

        std::unordered_set<KeyType> visited;
        std::queue<KeyType> queue;

        queue.push(toKey);
        visited.insert(toKey);

        while (!queue.empty() && !hasCycleResult) {
            KeyType current = queue.front();
            queue.pop();

            if (current == fromKey) {
                hasCycleResult = true;
                break;
            }

            for (const auto& nextNode : outEdges_[current]) {
                if (visited.insert(nextNode).second) {
                    queue.push(nextNode);
                }
            }
        }

        if (hasCycleResult) {
            outEdges_[fromKey].erase(toKey);
            inEdges_[toKey].erase(fromKey);
            throw CycleDetectedException("Adding this edge would create a cycle in the graph");
        }

        return true;
    }

    bool removeEdge(const KeyType& fromKey, const KeyType& toKey) {
        auto lock = lockGraph(LockIntent::GraphStructure);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for edge removal");
        }

        if (nodes_.find(fromKey) == nodes_.end() || nodes_.find(toKey) == nodes_.end()) {
            return false;
        }

        if (outEdges_[fromKey].find(toKey) == outEdges_[fromKey].end()) {
            return false;
        }

        outEdges_[fromKey].erase(toKey);
        inEdges_[toKey].erase(fromKey);

        return true;
    }

    bool hasEdge(const KeyType& fromKey, const KeyType& toKey) const {
        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for edge check");
        }

        if (nodes_.find(fromKey) == nodes_.end() || nodes_.find(toKey) == nodes_.end()) {
            return false;
        }

        return outEdges_.at(fromKey).find(toKey) != outEdges_.at(fromKey).end();
    }

    std::unordered_set<KeyType> getOutEdges(const KeyType& key) const {
        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for retrieving outgoing edges");
        }

        if (outEdges_.find(key) == outEdges_.end()) {
            return {};
        }

        return outEdges_.at(key);
    }

    std::unordered_set<KeyType> getInEdges(const KeyType& key) const {
        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for retrieving incoming edges");
        }

        if (inEdges_.find(key) == inEdges_.end()) {
            return {};
        }

        return inEdges_.at(key);
    }

    // -----------------------------------------------------------------------
    // Traversal and query
    // -----------------------------------------------------------------------

    bool hasCycle() const {
        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for cycle detection");
        }

        if (nodes_.size() <= 1) {
            return false;
        }

        std::unordered_map<KeyType, NodeState> visited;

        for (const auto& node : nodes_) {
            if (visited.find(node.first) == visited.end()) {
                if (hasCycleInternal(node.first, visited)) {
                    return true;
                }
            }
        }

        return false;
    }

    std::vector<KeyType> topologicalSort() const {
        std::unordered_map<KeyType, std::unordered_set<KeyType>> localOutEdges;
        std::unordered_set<KeyType> localNodes;

        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for topological sort");
        }

        if (nodes_.empty()) {
            return {};
        }

        for (const auto& [key, _] : nodes_) {
            localNodes.insert(key);

            auto edgeIt = outEdges_.find(key);
            if (edgeIt != outEdges_.end()) {
                localOutEdges[key] = edgeIt->second;
            } else {
                localOutEdges[key] = {};
            }
        }

        lock->release();

        std::vector<KeyType> result;
        std::unordered_map<KeyType, bool> visited;
        std::unordered_map<KeyType, bool> inProcess;

        std::function<bool(const KeyType&)> visit = [&](const KeyType& key) {
            if (inProcess[key]) {
                return false;
            }

            if (visited[key]) {
                return true;
            }

            inProcess[key] = true;

            auto edgeIt = localOutEdges.find(key);
            if (edgeIt != localOutEdges.end()) {
                for (const auto& neighbor : edgeIt->second) {
                    if (localNodes.find(neighbor) == localNodes.end()) {
                        continue;
                    }

                    if (!visit(neighbor)) {
                        return false;
                    }
                }
            }

            inProcess[key] = false;
            visited[key] = true;
            result.push_back(key);

            return true;
        };

        for (const auto& node : localNodes) {
            if (!visited[node]) {
                if (!visit(node)) {
                    return {};
                }
            }
        }

        std::reverse(result.begin(), result.end());
        return result;
    }

    template <typename Func>
    bool withNode(const KeyType& key, Func&& func, bool forWrite = false, size_t timeoutMs = 100) {
        auto intent = forWrite ? LockIntent::NodeModify : LockIntent::Read;
        auto nodeLock = tryLockNode(key, intent, forWrite, timeoutMs);

        if (!nodeLock || !nodeLock->isLocked()) {
            return false;
        }

        auto node = nodeLock->getNode();
        if (!node) {
            return false;
        }

        if constexpr (std::is_invocable_v<Func, T&>) {
            if (forWrite) {
                func(node->getDataNoLock());
            } else {
                func(const_cast<T&>(node->getDataNoLock()));
            }
        } else if constexpr (std::is_invocable_v<Func, const T&>) {
            func(static_cast<const T&>(node->getDataNoLock()));
        } else {
            static_assert(std::is_invocable_v<Func, T&> || std::is_invocable_v<Func, const T&>,
                          "Function must accept either T& or const T&");
        }

        return true;
    }

    bool processDependencyOrder(std::function<void(const KeyType&, T&)> processFunc) {
        std::vector<KeyType> sortedNodes = topologicalSort();

        if (sortedNodes.empty()) {
            auto lock = lockGraph(LockIntent::Read);
            if (lock && lock->isLocked() && !nodes_.empty()) {
                return false;
            }
            return true;
        }

        for (const auto& key : sortedNodes) {
            auto nodeLock = tryLockNode(key, LockIntent::NodeModify, true, 100);
            if (!nodeLock || !nodeLock->isLocked()) {
                continue;
            }

            auto node = nodeLock->getNode();
            if (!node) {
                continue;
            }

            processFunc(key, node->getDataNoLock());
        }

        return true;
    }

    void bfs(const KeyType& startKey, std::function<void(const KeyType&, const T&)> visitFunc) const {
        std::unordered_map<KeyType, std::unordered_set<KeyType>> localOutEdges;
        std::unordered_map<KeyType, std::shared_ptr<Node>> localNodes;

        {
            auto lock = lockGraph(LockIntent::Read);
            if (!lock || !lock->isLocked()) {
                throw LockAcquisitionException("Failed to acquire graph lock for BFS");
            }

            auto nodeIt = nodes_.find(startKey);
            if (nodeIt == nodes_.end()) {
                return;
            }

            localNodes[startKey] = nodeIt->second;

            auto edgeIt = outEdges_.find(startKey);
            if (edgeIt != outEdges_.end()) {
                localOutEdges[startKey] = edgeIt->second;
            }
        }

        std::queue<KeyType> queue;
        std::unordered_set<KeyType> visited;

        queue.push(startKey);
        visited.insert(startKey);

        {
            auto nodeLock = tryLockNode(startKey, LockIntent::Read, false, 50);
            if (nodeLock && nodeLock->isLocked()) {
                auto node = nodeLock->getNode();
                if (node) {
                    visitFunc(startKey, node->getData());
                }
            }
        }

        while (!queue.empty()) {
            KeyType current = queue.front();
            queue.pop();

            if (localOutEdges.find(current) == localOutEdges.end()) {
                auto lock = lockGraph(LockIntent::Read);
                if (!lock || !lock->isLocked()) {
                    continue;
                }

                auto edgeIt = outEdges_.find(current);
                if (edgeIt != outEdges_.end()) {
                    localOutEdges[current] = edgeIt->second;
                } else {
                    localOutEdges[current] = {};
                }
            }

            for (const auto& neighbor : localOutEdges[current]) {
                if (visited.insert(neighbor).second) {
                    queue.push(neighbor);

                    if (localNodes.find(neighbor) == localNodes.end()) {
                        auto lock = lockGraph(LockIntent::Read);
                        if (!lock || !lock->isLocked()) {
                            continue;
                        }

                        auto nodeIt = nodes_.find(neighbor);
                        if (nodeIt != nodes_.end()) {
                            localNodes[neighbor] = nodeIt->second;
                        }
                    }

                    auto nodeLock = tryLockNode(neighbor, LockIntent::Read, false, 50);
                    if (nodeLock && nodeLock->isLocked()) {
                        auto node = nodeLock->getNode();
                        if (node) {
                            visitFunc(neighbor, node->getData());
                        }
                    }
                }
            }
        }
    }

    void dfs(const KeyType& startKey, std::function<void(const KeyType&, const T&)> visitFunc) const {
        std::unordered_map<KeyType, std::unordered_set<KeyType>> localOutEdges;

        {
            auto lock = lockGraph(LockIntent::Read);
            if (!lock || !lock->isLocked()) {
                throw LockAcquisitionException("Failed to acquire graph lock for DFS");
            }

            auto nodeIt = nodes_.find(startKey);
            if (nodeIt == nodes_.end()) {
                return;
            }

            auto edgeIt = outEdges_.find(startKey);
            if (edgeIt != outEdges_.end()) {
                localOutEdges[startKey] = edgeIt->second;
            }
        }

        std::unordered_set<KeyType> visited;
        std::stack<KeyType> stack;

        stack.push(startKey);

        while (!stack.empty()) {
            KeyType current = stack.top();
            stack.pop();

            if (visited.find(current) != visited.end()) {
                continue;
            }

            visited.insert(current);

            auto nodeLock = tryLockNode(current, LockIntent::Read, false, 50);
            if (nodeLock && nodeLock->isLocked()) {
                auto node = nodeLock->getNode();
                if (node) {
                    visitFunc(current, node->getData());
                }
            }

            if (localOutEdges.find(current) == localOutEdges.end()) {
                auto lock = lockGraph(LockIntent::Read);
                if (!lock || !lock->isLocked()) {
                    continue;
                }

                auto edgeIt = outEdges_.find(current);
                if (edgeIt != outEdges_.end()) {
                    localOutEdges[current] = edgeIt->second;
                } else {
                    localOutEdges[current] = {};
                }
            }

            std::vector<KeyType> neighbors(localOutEdges[current].begin(), localOutEdges[current].end());
            for (auto it = neighbors.rbegin(); it != neighbors.rend(); ++it) {
                if (visited.find(*it) == visited.end()) {
                    stack.push(*it);
                }
            }
        }
    }

    std::vector<KeyType> getAllNodes() const {
        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for getting all nodes");
        }

        std::vector<KeyType> keys;
        keys.reserve(nodes_.size());

        for (const auto& node : nodes_) {
            keys.push_back(node.first);
        }

        return keys;
    }

    size_t size() const {
        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for getting size");
        }

        return nodes_.size();
    }

    bool empty() const {
        auto lock = lockGraph(LockIntent::Read);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for checking emptiness");
        }

        return nodes_.empty();
    }

    void clear() {
        auto lock = lockGraph(LockIntent::GraphStructure);
        if (!lock || !lock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for clearing");
        }

        for (const auto& [_, node] : nodes_) {
            if (node) {
                node->notifyLockHolders(LockStatus::Preempted);
            }
        }

        nodes_.clear();
        outEdges_.clear();
        inEdges_.clear();
    }

    // -----------------------------------------------------------------------
    // Callbacks
    // -----------------------------------------------------------------------

    std::string registerNodeRemovalCallback(std::function<void(const KeyType&)> callback) {
        std::string id = std::to_string(++callbackCounter_);
        std::lock_guard<std::mutex> lock(callbackMutex_);
        removalCallbacks_[id] = std::move(callback);
        return id;
    }

    bool unregisterNodeRemovalCallback(const std::string& id) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        auto it = removalCallbacks_.find(id);
        if (it != removalCallbacks_.end()) {
            removalCallbacks_.erase(it);
            return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Resource locking configuration
    // -----------------------------------------------------------------------

    void setLockHistoryEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(lockGraphMutex_);
        lockHistoryEnabled_ = enabled;
    }

    void setDeadlockDetectionEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(lockGraphMutex_);
        deadlockDetectionEnabled_ = enabled;
    }

    // -----------------------------------------------------------------------
    // Resource locking operations (defined in CoordinatedGraphLocking.hh)
    // -----------------------------------------------------------------------

    std::unique_ptr<ResourceLockHandle> tryLockResource(const KeyType& resourceKey, LockMode mode,
                                                        size_t timeoutMs = 100);

    bool releaseResourceLock(const KeyType& resourceKey, LockMode mode,
                             std::thread::id threadId = std::this_thread::get_id());

    bool upgradeResourceLock(const KeyType& resourceKey, std::thread::id threadId = std::this_thread::get_id(),
                             size_t timeoutMs = 100);

    bool hasLock(const KeyType& resourceKey, std::thread::id threadId = std::this_thread::get_id()) const;

    ResourceLockStatus getLockStatus(const KeyType& resourceKey,
                                     std::thread::id threadId = std::this_thread::get_id()) const;

    std::vector<std::unique_ptr<ResourceLockHandle>> tryLockResourcesInOrder(const std::vector<KeyType>& resources,
                                                                             LockMode mode, size_t timeoutMs = 100);

    std::vector<std::tuple<std::string, KeyType, std::thread::id, std::chrono::steady_clock::time_point, LockMode>>
    getLockHistory() const;

    void clearLockHistory();

  private:
    friend class GraphLockHandle;
    friend class NodeLockHandle;
    friend class ResourceLockHandle;

    // -----------------------------------------------------------------------
    // Private core helpers
    // -----------------------------------------------------------------------

    void onGraphLockReleased(LockIntent intent) {
        if (intent == LockIntent::GraphStructure) {
            currentStructuralIntent_.store(-1, std::memory_order_release);
            notifyAllNodeLockHolders(LockStatus::Acquired);
        }
    }

    void notifyAllNodeLockHolders(LockStatus status) {
        std::lock_guard<std::shared_mutex> lock(graphMutex_);
        for (const auto& [_, node] : nodes_) {
            if (node) {
                node->notifyLockHolders(status);
            }
        }
    }

    bool canProceedWithIntent(LockIntent intent) const {
        int current = currentStructuralIntent_.load(std::memory_order_acquire);
        if (current >= 0 && static_cast<LockIntent>(current) == LockIntent::GraphStructure &&
            intent != LockIntent::Read) {
            return false;
        }
        return true;
    }

    void onNodeRemoved(const KeyType& key) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        for (const auto& [_, callback] : removalCallbacks_) {
            if (callback) {
                callback(key);
            }
        }
    }

    bool hasCycleInternal(const KeyType& key, std::unordered_map<KeyType, NodeState>& visited) const {
        visited[key] = NodeState::Visiting;

        auto it = outEdges_.find(key);
        if (it == outEdges_.end()) {
            visited[key] = NodeState::Visited;
            return false;
        }

        const auto& neighbors = it->second;
        for (const auto& neighbor : neighbors) {
            if (nodes_.find(neighbor) == nodes_.end()) {
                continue;
            }

            if (visited.find(neighbor) == visited.end()) {
                if (hasCycleInternal(neighbor, visited)) {
                    return true;
                }
            } else if (visited[neighbor] == NodeState::Visiting) {
                return true;
            }
        }

        visited[key] = NodeState::Visited;
        return false;
    }

    // -----------------------------------------------------------------------
    // Private resource locking helpers (defined in CoordinatedGraphLocking.hh)
    // -----------------------------------------------------------------------

    bool wouldCauseDeadlock(const KeyType& resourceKey, std::thread::id threadId);

    std::unordered_map<KeyType, std::unordered_set<KeyType>>
    buildResourceLockSubgraph(const std::vector<KeyType>& resources);

    std::vector<KeyType>
    getTopologicalOrderForResources(const std::unordered_map<KeyType, std::unordered_set<KeyType>>& subgraph);

    // -----------------------------------------------------------------------
    // Member variables
    // -----------------------------------------------------------------------

    // Core graph state
    mutable std::shared_mutex graphMutex_;
    std::unordered_map<KeyType, std::shared_ptr<Node>> nodes_;
    std::unordered_map<KeyType, std::unordered_set<KeyType>> outEdges_;
    std::unordered_map<KeyType, std::unordered_set<KeyType>> inEdges_;

    // Node removal callbacks
    std::mutex callbackMutex_;
    std::unordered_map<std::string, std::function<void(const KeyType&)>> removalCallbacks_;
    std::atomic<size_t> callbackCounter_{0};

    // Structural intent tracking
    std::atomic<int> currentStructuralIntent_{-1};

    // Resource locking state
    mutable std::mutex lockGraphMutex_;
    std::unordered_map<KeyType, std::unordered_map<std::thread::id, std::unique_ptr<NodeLockHandle>>>
        resourceNodeLocks_;
    std::unordered_map<std::thread::id, std::unordered_set<KeyType>> threadResourceMap_;
    std::unordered_map<KeyType, std::unordered_map<std::thread::id, ResourceLockStatus>> resourceLockStatus_;
    std::vector<std::tuple<std::string, KeyType, std::thread::id, std::chrono::steady_clock::time_point, LockMode>>
        lockHistory_;
    bool lockHistoryEnabled_ = false;
    bool deadlockDetectionEnabled_ = true;
};

} // namespace fabric

// Out-of-class template definitions for resource locking operations
#include "fabric/utils/CoordinatedGraphLocking.hh"

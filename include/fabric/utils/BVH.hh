#pragma once

#include "fabric/core/Rendering.hh"
#include <vector>
#include <algorithm>
#include <functional>
#include <limits>

namespace fabric {

template <typename T>
class BVH {
public:
    void insert(const AABB& bounds, T data) {
        items_.push_back({bounds, std::move(data)});
        dirty_ = true;
    }

    bool remove(const T& data) {
        for (auto it = items_.begin(); it != items_.end(); ++it) {
            if (it->data == data) {
                items_.erase(it);
                dirty_ = true;
                return true;
            }
        }
        return false;
    }

    void build() {
        nodes_.clear();
        root_ = -1;

        if (items_.empty()) {
            dirty_ = false;
            return;
        }

        std::vector<int> indices(items_.size());
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            indices[i] = i;
        }

        // Reserve worst-case nodes (2n - 1 for n leaves)
        nodes_.reserve(items_.size() * 2);
        nodes_.push_back(Node{});
        root_ = 0;

        buildRecursive(0, indices, 0, static_cast<int>(indices.size()));
        dirty_ = false;
    }

    std::vector<T> query(const AABB& region) const {
        if (dirty_) {
            const_cast<BVH*>(this)->build();
        }

        std::vector<T> results;
        if (root_ >= 0) {
            queryRecursive(root_, region, results);
        }
        return results;
    }

    std::vector<T> queryFrustum(const Frustum& frustum) const {
        if (dirty_) {
            const_cast<BVH*>(this)->build();
        }

        std::vector<T> results;
        if (root_ >= 0) {
            queryFrustumRecursive(root_, frustum, results);
        }
        return results;
    }

    void clear() {
        items_.clear();
        nodes_.clear();
        root_ = -1;
        dirty_ = true;
    }

    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

private:
    struct Item {
        AABB bounds;
        T data;
    };

    struct Node {
        AABB bounds;
        int left = -1;
        int right = -1;
        int itemIndex = -1;
    };

    std::vector<Item> items_;
    std::vector<Node> nodes_;
    int root_ = -1;
    bool dirty_ = true;

    void buildRecursive(int nodeIndex, std::vector<int>& indices, int start, int end) {
        // Compute bounding box for all items in [start, end)
        AABB nodeBounds = items_[indices[start]].bounds;
        for (int i = start + 1; i < end; ++i) {
            nodeBounds = unionAABB(nodeBounds, items_[indices[i]].bounds);
        }
        nodes_[nodeIndex].bounds = nodeBounds;

        // Leaf: single item
        if (end - start == 1) {
            nodes_[nodeIndex].itemIndex = indices[start];
            return;
        }

        // Find longest axis of the node's bounding box
        float dx = nodeBounds.max.x - nodeBounds.min.x;
        float dy = nodeBounds.max.y - nodeBounds.min.y;
        float dz = nodeBounds.max.z - nodeBounds.min.z;

        int axis = 0;
        if (dy > dx && dy > dz) axis = 1;
        else if (dz > dx && dz > dy) axis = 2;

        // Sort indices by centroid along chosen axis
        std::sort(indices.begin() + start, indices.begin() + end,
            [&](int a, int b) {
                Vec3f ca = items_[a].bounds.center();
                Vec3f cb = items_[b].bounds.center();
                if (axis == 0) return ca.x < cb.x;
                if (axis == 1) return ca.y < cb.y;
                return ca.z < cb.z;
            });

        int mid = start + (end - start) / 2;

        // Allocate child nodes
        nodes_.push_back(Node{});
        int leftIndex = static_cast<int>(nodes_.size()) - 1;
        nodes_.push_back(Node{});
        int rightIndex = static_cast<int>(nodes_.size()) - 1;

        nodes_[nodeIndex].left = leftIndex;
        nodes_[nodeIndex].right = rightIndex;

        buildRecursive(leftIndex, indices, start, mid);
        buildRecursive(rightIndex, indices, mid, end);
    }

    void queryRecursive(int nodeIndex, const AABB& region, std::vector<T>& results) const {
        const auto& node = nodes_[nodeIndex];

        if (!node.bounds.intersects(region)) {
            return;
        }

        // Leaf node
        if (node.itemIndex >= 0) {
            results.push_back(items_[node.itemIndex].data);
            return;
        }

        if (node.left >= 0) queryRecursive(node.left, region, results);
        if (node.right >= 0) queryRecursive(node.right, region, results);
    }

    void queryFrustumRecursive(int nodeIndex, const Frustum& frustum, std::vector<T>& results) const {
        const auto& node = nodes_[nodeIndex];

        if (frustum.testAABB(node.bounds) == CullResult::Outside) {
            return;
        }

        // Leaf node
        if (node.itemIndex >= 0) {
            results.push_back(items_[node.itemIndex].data);
            return;
        }

        if (node.left >= 0) queryFrustumRecursive(node.left, frustum, results);
        if (node.right >= 0) queryFrustumRecursive(node.right, frustum, results);
    }

    static AABB unionAABB(const AABB& a, const AABB& b) {
        return AABB(
            Vec3f(std::min(a.min.x, b.min.x),
                  std::min(a.min.y, b.min.y),
                  std::min(a.min.z, b.min.z)),
            Vec3f(std::max(a.max.x, b.max.x),
                  std::max(a.max.y, b.max.y),
                  std::max(a.max.z, b.max.z))
        );
    }
};

} // namespace fabric

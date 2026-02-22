#pragma once

#include "fabric/utils/ErrorHandling.hh"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <queue>
#include <stack>
#include <unordered_set>
#include <vector>

namespace fabric {

using NodeId = size_t;

// Lightweight append-only directed acyclic graph.
// No locking: the caller ensures single-writer access.
// Designed for commit-DAG-like structures where nodes are appended
// and never mutated, edges are added infrequently, and reads are frequent.
template <typename NodeData>
class ImmutableDAG {
public:
  // Append a new node. Returns its ID.
  NodeId addNode(NodeData data) {
    NodeId id = nodes_.size();
    nodes_.push_back(Node{std::move(data), {}, {}});
    return id;
  }

  // Add a directed edge from -> to. Throws if the edge would create a cycle.
  void addEdge(NodeId from, NodeId to) {
    validateId(from);
    validateId(to);
    if (from == to) {
      throwError("ImmutableDAG: self-loop on node " + std::to_string(from));
    }
    // Check if adding from->to would create a cycle.
    // A cycle exists iff 'to' can already reach 'from'.
    if (isReachableInternal(to, from)) {
      throwError("ImmutableDAG: adding edge " + std::to_string(from) +
                 " -> " + std::to_string(to) + " would create a cycle");
    }
    nodes_[from].children.push_back(to);
    nodes_[to].parents.push_back(from);
    ++edgeCount_;
  }

  size_t nodeCount() const { return nodes_.size(); }
  size_t edgeCount() const { return edgeCount_; }

  const NodeData& getData(NodeId id) const {
    validateId(id);
    return nodes_[id].data;
  }

  std::vector<NodeId> getParents(NodeId id) const {
    validateId(id);
    return nodes_[id].parents;
  }

  std::vector<NodeId> getChildren(NodeId id) const {
    validateId(id);
    return nodes_[id].children;
  }

  // Breadth-first traversal from start. Visitor returns false to stop.
  void bfs(NodeId start, std::function<bool(NodeId)> visitor) const {
    validateId(start);
    std::vector<bool> visited(nodes_.size(), false);
    std::queue<NodeId> q;
    q.push(start);
    visited[start] = true;
    while (!q.empty()) {
      NodeId cur = q.front();
      q.pop();
      if (!visitor(cur))
        return;
      for (NodeId child : nodes_[cur].children) {
        if (!visited[child]) {
          visited[child] = true;
          q.push(child);
        }
      }
    }
  }

  // Depth-first traversal from start. Visitor returns false to stop.
  void dfs(NodeId start, std::function<bool(NodeId)> visitor) const {
    validateId(start);
    std::vector<bool> visited(nodes_.size(), false);
    std::stack<NodeId> s;
    s.push(start);
    while (!s.empty()) {
      NodeId cur = s.top();
      s.pop();
      if (visited[cur])
        continue;
      visited[cur] = true;
      if (!visitor(cur))
        return;
      // Push children in reverse so leftmost child is visited first
      const auto& ch = nodes_[cur].children;
      for (auto it = ch.rbegin(); it != ch.rend(); ++it) {
        if (!visited[*it])
          s.push(*it);
      }
    }
  }

  // Kahn's algorithm: returns all nodes in topological order.
  std::vector<NodeId> topologicalSort() const {
    size_t n = nodes_.size();
    std::vector<size_t> inDeg(n, 0);
    for (size_t i = 0; i < n; ++i) {
      for (NodeId child : nodes_[i].children) {
        ++inDeg[child];
      }
    }
    std::queue<NodeId> ready;
    for (size_t i = 0; i < n; ++i) {
      if (inDeg[i] == 0)
        ready.push(i);
    }
    std::vector<NodeId> result;
    result.reserve(n);
    while (!ready.empty()) {
      NodeId cur = ready.front();
      ready.pop();
      result.push_back(cur);
      for (NodeId child : nodes_[cur].children) {
        if (--inDeg[child] == 0)
          ready.push(child);
      }
    }
    return result;
  }

  // Lowest common ancestor of a and b, traversing parents.
  // Uses the set-intersection approach: walk ancestors of a and b
  // in BFS order, return the first node reachable from both.
  std::optional<NodeId> lca(NodeId a, NodeId b) const {
    validateId(a);
    validateId(b);
    if (a == b)
      return a;

    // Collect all ancestors of a (inclusive)
    std::unordered_set<NodeId> ancestorsA;
    {
      std::queue<NodeId> q;
      q.push(a);
      ancestorsA.insert(a);
      while (!q.empty()) {
        NodeId cur = q.front();
        q.pop();
        for (NodeId p : nodes_[cur].parents) {
          if (ancestorsA.insert(p).second)
            q.push(p);
        }
      }
    }

    // BFS from b upward; first hit in ancestorsA is the LCA
    std::queue<NodeId> q;
    std::unordered_set<NodeId> visitedB;
    q.push(b);
    visitedB.insert(b);
    while (!q.empty()) {
      NodeId cur = q.front();
      q.pop();
      if (ancestorsA.contains(cur))
        return cur;
      for (NodeId p : nodes_[cur].parents) {
        if (visitedB.insert(p).second)
          q.push(p);
      }
    }
    return std::nullopt;
  }

  // Returns true if 'to' is reachable from 'from' via directed edges.
  bool isReachable(NodeId from, NodeId to) const {
    validateId(from);
    validateId(to);
    return isReachableInternal(from, to);
  }

private:
  struct Node {
    NodeData data;
    std::vector<NodeId> parents;
    std::vector<NodeId> children;
  };

  void validateId(NodeId id) const {
    if (id >= nodes_.size()) {
      throwError("ImmutableDAG: invalid node ID " + std::to_string(id));
    }
  }

  bool isReachableInternal(NodeId from, NodeId to) const {
    if (from == to)
      return true;
    std::vector<bool> visited(nodes_.size(), false);
    std::queue<NodeId> q;
    q.push(from);
    visited[from] = true;
    while (!q.empty()) {
      NodeId cur = q.front();
      q.pop();
      for (NodeId child : nodes_[cur].children) {
        if (child == to)
          return true;
        if (!visited[child]) {
          visited[child] = true;
          q.push(child);
        }
      }
    }
    return false;
  }

  std::vector<Node> nodes_;
  size_t edgeCount_ = 0;
};

} // namespace fabric

#include "fabric/utils/ImmutableDAG.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace fabric;

TEST(ImmutableDAGTest, AddNodesAndEdges) {
    ImmutableDAG<std::string> dag;
    auto a = dag.addNode("A");
    auto b = dag.addNode("B");
    auto c = dag.addNode("C");

    EXPECT_EQ(dag.nodeCount(), 3u);
    EXPECT_EQ(dag.edgeCount(), 0u);

    dag.addEdge(a, b);
    dag.addEdge(b, c);
    EXPECT_EQ(dag.edgeCount(), 2u);

    EXPECT_EQ(dag.getData(a), "A");
    EXPECT_EQ(dag.getData(b), "B");
    EXPECT_EQ(dag.getData(c), "C");
}

TEST(ImmutableDAGTest, ParentsAndChildren) {
    ImmutableDAG<int> dag;
    auto a = dag.addNode(1);
    auto b = dag.addNode(2);
    auto c = dag.addNode(3);
    dag.addEdge(a, b);
    dag.addEdge(a, c);

    auto childrenA = dag.getChildren(a);
    EXPECT_EQ(childrenA.size(), 2u);

    auto parentsB = dag.getParents(b);
    EXPECT_EQ(parentsB.size(), 1u);
    EXPECT_EQ(parentsB[0], a);

    auto parentsA = dag.getParents(a);
    EXPECT_TRUE(parentsA.empty());
}

TEST(ImmutableDAGTest, CycleDetection) {
    ImmutableDAG<int> dag;
    auto a = dag.addNode(0);
    auto b = dag.addNode(1);
    auto c = dag.addNode(2);
    dag.addEdge(a, b);
    dag.addEdge(b, c);

    // c -> a would create a cycle
    EXPECT_THROW(dag.addEdge(c, a), FabricException);
    EXPECT_EQ(dag.edgeCount(), 2u); // edge was not added
}

TEST(ImmutableDAGTest, SelfLoopThrows) {
    ImmutableDAG<int> dag;
    auto a = dag.addNode(0);
    EXPECT_THROW(dag.addEdge(a, a), FabricException);
}

TEST(ImmutableDAGTest, InvalidNodeThrows) {
    ImmutableDAG<int> dag;
    dag.addNode(0);
    EXPECT_THROW(dag.getData(99), FabricException);
    EXPECT_THROW(dag.addEdge(0, 99), FabricException);
}

TEST(ImmutableDAGTest, BFS) {
    //   0
    //  / \
    // 1   2
    //      \
    //       3
    ImmutableDAG<int> dag;
    auto n0 = dag.addNode(0);
    auto n1 = dag.addNode(1);
    auto n2 = dag.addNode(2);
    auto n3 = dag.addNode(3);
    dag.addEdge(n0, n1);
    dag.addEdge(n0, n2);
    dag.addEdge(n2, n3);

    std::vector<NodeId> visited;
    dag.bfs(n0, [&](NodeId id) {
        visited.push_back(id);
        return true;
    });

    ASSERT_EQ(visited.size(), 4u);
    EXPECT_EQ(visited[0], n0);
    // n1 and n2 are at the same level; order depends on child insertion
    EXPECT_EQ(visited[1], n1);
    EXPECT_EQ(visited[2], n2);
    EXPECT_EQ(visited[3], n3);
}

TEST(ImmutableDAGTest, BFSEarlyStop) {
    ImmutableDAG<int> dag;
    auto n0 = dag.addNode(0);
    auto n1 = dag.addNode(1);
    auto n2 = dag.addNode(2);
    dag.addEdge(n0, n1);
    dag.addEdge(n0, n2);

    std::vector<NodeId> visited;
    dag.bfs(n0, [&](NodeId id) {
        visited.push_back(id);
        return id != n1; // stop after visiting n1
    });

    EXPECT_EQ(visited.size(), 2u); // n0, n1
}

TEST(ImmutableDAGTest, DFS) {
    //   0
    //  / \
    // 1   2
    //      \
    //       3
    ImmutableDAG<int> dag;
    auto n0 = dag.addNode(0);
    auto n1 = dag.addNode(1);
    auto n2 = dag.addNode(2);
    auto n3 = dag.addNode(3);
    dag.addEdge(n0, n1);
    dag.addEdge(n0, n2);
    dag.addEdge(n2, n3);

    std::vector<NodeId> visited;
    dag.dfs(n0, [&](NodeId id) {
        visited.push_back(id);
        return true;
    });

    ASSERT_EQ(visited.size(), 4u);
    EXPECT_EQ(visited[0], n0);
    // DFS goes deep first: 0 -> 1, then 2 -> 3
    EXPECT_EQ(visited[1], n1);
    EXPECT_EQ(visited[2], n2);
    EXPECT_EQ(visited[3], n3);
}

TEST(ImmutableDAGTest, TopologicalSort) {
    //   0 --> 1 --> 3
    //   |           ^
    //   v           |
    //   2 ----------+
    ImmutableDAG<int> dag;
    auto n0 = dag.addNode(0);
    auto n1 = dag.addNode(1);
    auto n2 = dag.addNode(2);
    auto n3 = dag.addNode(3);
    dag.addEdge(n0, n1);
    dag.addEdge(n0, n2);
    dag.addEdge(n1, n3);
    dag.addEdge(n2, n3);

    auto sorted = dag.topologicalSort();
    ASSERT_EQ(sorted.size(), 4u);

    // n0 must come before n1 and n2; n3 must come last
    auto pos = [&](NodeId id) {
        for (size_t i = 0; i < sorted.size(); ++i) {
            if (sorted[i] == id)
                return i;
        }
        return sorted.size();
    };
    EXPECT_LT(pos(n0), pos(n1));
    EXPECT_LT(pos(n0), pos(n2));
    EXPECT_LT(pos(n1), pos(n3));
    EXPECT_LT(pos(n2), pos(n3));
}

TEST(ImmutableDAGTest, LCA) {
    //       0
    //      / \
    //     1   2
    //    / \
    //   3   4
    ImmutableDAG<int> dag;
    auto n0 = dag.addNode(0);
    auto n1 = dag.addNode(1);
    auto n2 = dag.addNode(2);
    auto n3 = dag.addNode(3);
    auto n4 = dag.addNode(4);
    dag.addEdge(n0, n1);
    dag.addEdge(n0, n2);
    dag.addEdge(n1, n3);
    dag.addEdge(n1, n4);

    // LCA of 3 and 4 is 1
    auto result = dag.lca(n3, n4);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, n1);

    // LCA of 3 and 2 is 0
    result = dag.lca(n3, n2);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, n0);

    // LCA of same node
    result = dag.lca(n3, n3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, n3);
}

TEST(ImmutableDAGTest, LCADisconnected) {
    ImmutableDAG<int> dag;
    auto a = dag.addNode(0);
    auto b = dag.addNode(1);
    // No edges: disconnected
    auto result = dag.lca(a, b);
    EXPECT_FALSE(result.has_value());
}

TEST(ImmutableDAGTest, IsReachable) {
    ImmutableDAG<int> dag;
    auto a = dag.addNode(0);
    auto b = dag.addNode(1);
    auto c = dag.addNode(2);
    auto d = dag.addNode(3);
    dag.addEdge(a, b);
    dag.addEdge(b, c);

    EXPECT_TRUE(dag.isReachable(a, c));
    EXPECT_TRUE(dag.isReachable(a, b));
    EXPECT_FALSE(dag.isReachable(c, a)); // reverse
    EXPECT_FALSE(dag.isReachable(a, d)); // disconnected
    EXPECT_TRUE(dag.isReachable(a, a));  // self
}

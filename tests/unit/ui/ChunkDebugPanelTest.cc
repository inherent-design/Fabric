#include "recurse/ui/ChunkDebugPanel.hh"

#include <gtest/gtest.h>

using namespace recurse;

TEST(ChunkDebugDataTest, DefaultInitialization) {
    ChunkDebugData data;
    EXPECT_EQ(data.trackedChunks, 0u);
    EXPECT_EQ(data.activeChunks, 0u);
    EXPECT_EQ(data.meshCandidateChunks, 0u);
    EXPECT_EQ(data.lodVisibleSections, 0u);
    EXPECT_EQ(data.lodGpuSectionCount, 0u);
    EXPECT_EQ(data.lodPendingSections, 0u);
}

TEST(ChunkDebugPanelTest, UpdateWithoutInitDoesNotCrash) {
    ChunkDebugPanel panel;
    ChunkDebugData data;
    data.trackedChunks = 12;
    data.activeChunks = 3;
    data.meshCandidateChunks = 5;
    data.lodVisibleSections = 2;
    panel.update(data);
}

TEST(ChunkDebugPanelTest, InitWithNullContextDoesNotCrash) {
    ChunkDebugPanel panel;
    panel.init(nullptr);
    EXPECT_FALSE(panel.isVisible());
}

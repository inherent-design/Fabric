#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open())
        return {};
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

TEST(WorldTransitionAssetsTest, MarkupKeepsTitleStageDetailInSeparateBlocks) {
    const std::string rml = readFile("assets/ui/world_transition.rml");
    ASSERT_FALSE(rml.empty());

    const size_t titlePos = rml.find("class=\"transition-title\"");
    const size_t stagePos = rml.find("class=\"transition-stage\"");
    const size_t detailPos = rml.find("class=\"transition-detail\"");

    ASSERT_NE(titlePos, std::string::npos);
    ASSERT_NE(stagePos, std::string::npos);
    ASSERT_NE(detailPos, std::string::npos);
    EXPECT_LT(titlePos, stagePos);
    EXPECT_LT(stagePos, detailPos);
}

TEST(WorldTransitionAssetsTest, CardUsesExplicitColumnGapForTextSeparation) {
    const std::string rcss = readFile("assets/ui/world_transition.rcss");
    ASSERT_FALSE(rcss.empty());

    EXPECT_NE(rcss.find("#world-transition-card {"), std::string::npos);
    EXPECT_NE(rcss.find("display: flex;"), std::string::npos);
    EXPECT_NE(rcss.find("flex-direction: column;"), std::string::npos);
    EXPECT_NE(rcss.find("gap: 10dp;"), std::string::npos);
}

#include "recurse/ui/AutosavePanel.hh"

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

using namespace recurse;

TEST(AutosavePanelTest, UpdateWithoutInitDoesNotCrash) {
    AutosavePanel panel;
    AutosaveIndicatorData data;
    data.visible = true;
    data.statusText = "Saving...";
    data.detailText = "Writing recent world changes";
    panel.update(data);
}

TEST(AutosavePanelAssetsTest, MarkupKeepsStatusAndDetailInSeparateBlocks) {
    const std::string rml = readFile("assets/ui/autosave.rml");
    ASSERT_FALSE(rml.empty());

    const size_t statusPos = rml.find("id=\"autosave-status\"");
    const size_t detailPos = rml.find("id=\"autosave-detail\"");

    ASSERT_NE(statusPos, std::string::npos);
    ASSERT_NE(detailPos, std::string::npos);
    EXPECT_LT(statusPos, detailPos);
}

TEST(AutosavePanelAssetsTest, StylesForceLineBreakAndPreserveDetailWhitespace) {
    const std::string rcss = readFile("assets/ui/autosave.rcss");
    ASSERT_FALSE(rcss.empty());

    EXPECT_NE(rcss.find(".autosave-copy {\n    display: block;"), std::string::npos);
    EXPECT_NE(rcss.find(".autosave-status {\n    color: #f0f0f0;\n    display: block;"), std::string::npos);
    EXPECT_NE(rcss.find("margin-bottom: 2dp;"), std::string::npos);
    EXPECT_NE(rcss.find(".autosave-detail {\n    color: #a8a8a8;\n    display: block;"), std::string::npos);
    EXPECT_NE(rcss.find("white-space: pre-wrap;"), std::string::npos);
}

#include "fabric/ui/WebView.hh"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

// Only compile the WebView tests when WebView is enabled
#if defined(FABRIC_USE_WEBVIEW)

namespace fabric {
namespace Tests {

class WebViewTest : public ::testing::Test {
  protected:
    // Mocked WebView for testing that doesn't actually create a window
    class TestableWebView : public WebView {
      public:
        TestableWebView(const std::string& title, int width, int height, bool debug)
            : WebView(title, width, height, debug,
                      false) // false = don't create the actual window
        {}

        // Expose internal state for testing
        std::string getTitle() const { return title; }
        int getWidth() const { return width; }
        int getHeight() const { return height; }
        bool isDebug() const { return debug; }
        std::string getHtml() const { return html; }
    };
};

TEST_F(WebViewTest, Initialization) {
    // Create a WebView
    TestableWebView webview("Test Window", 800, 600, true);

    // Verify initialization
    EXPECT_EQ(webview.getTitle(), "Test Window");
    EXPECT_EQ(webview.getWidth(), 800);
    EXPECT_EQ(webview.getHeight(), 600);
    EXPECT_TRUE(webview.isDebug());
}

TEST_F(WebViewTest, SetHtml) {
    // Create a WebView
    TestableWebView webview("Test Window", 800, 600, false);

    // Set HTML content
    const std::string testHtml = "<html><body><h1>Test Content</h1></body></html>";
    webview.setHTML(testHtml);

    // Verify HTML was set
    EXPECT_EQ(webview.getHtml(), testHtml);
}

} // namespace Tests
} // namespace fabric

#else

// Provide a placeholder test when WebView is disabled
namespace fabric {
namespace Tests {

TEST(WebViewTest, Disabled) {
    // This test only verifies that the test suite builds when WebView is disabled
    SUCCEED() << "WebView is disabled, tests skipped";
}

} // namespace Tests
} // namespace fabric

#endif

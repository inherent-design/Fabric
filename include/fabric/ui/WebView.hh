#pragma once

// Check if the FABRIC_USE_WEBVIEW macro is defined
#if defined(FABRIC_USE_WEBVIEW)
#include "webview/webview.h"
#endif

#include <functional>
#include <memory>
#include <string>

namespace fabric {

/**
 * @brief WebView wrapper class for the Fabric Engine
 *
 * This class encapsulates the webview functionality and provides
 * a clean interface for working with the embedded web browser.
 */
class WebView {
  public:
    /**
     * @brief Construct a new WebView object
     *
     * @param title Window title
     * @param width Window width
     * @param height Window height
     * @param debug Enable debug mode (shows developer tools)
     * @param createWindow Create actual window (false for testing)
     * @param window Parent window handle (nullptr for default)
     */
    WebView(const std::string& title = "Fabric", int width = 800, int height = 600, bool debug = false,
            bool createWindow = true, void* window = nullptr);

    /**
     * @brief Set the window title
     *
     * @param title Window title
     */
    void setTitle(const std::string& title);

    /**
     * @brief Set the window size
     *
     * @param width Window width
     * @param height Window height
     * @param hint Size hint (WEBVIEW_HINT_NONE, WEBVIEW_HINT_MIN,
     * WEBVIEW_HINT_MAX, WEBVIEW_HINT_FIXED)
     */
#if defined(FABRIC_USE_WEBVIEW)
    void setSize(int width, int height, webview_hint_t hint = WEBVIEW_HINT_NONE);
#else
    void setSize(int width, int height, int hint = 0);
#endif

    /**
     * @brief Navigate to a URL
     *
     * @param url URL to navigate to
     */
    void navigate(const std::string& url);

    /**
     * @brief Set HTML content directly
     *
     * @param html HTML content
     */
    void setHTML(const std::string& html);

    /**
     * @brief Run the main event loop
     */
    void run();

    /**
     * @brief Terminate the main event loop
     */
    void terminate();

    /**
     * @brief Evaluate JavaScript in the webview
     *
     * @param js JavaScript code to evaluate
     */
    void eval(const std::string& js);

    /**
     * @brief Bind a native C++ callback to be callable from JavaScript
     *
     * @param name Name of the function in JavaScript
     * @param fn Callback function
     */
    void bind(const std::string& name, const std::function<std::string(const std::string&)>& fn);

  protected:
    // These fields are protected to allow testing
    std::string title;
    int width;
    int height;
    bool debug;
    std::string html;

  private:
#if defined(FABRIC_USE_WEBVIEW)
    std::unique_ptr<webview::webview> webview_;
#endif
};

} // namespace fabric

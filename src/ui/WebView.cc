#include "fabric/ui/WebView.hh"
#include "fabric/core/Log.hh"
#include <memory>

#if defined(FABRIC_USE_WEBVIEW)

namespace fabric {

WebView::WebView(const std::string& title, int width, int height, bool debug, bool createWindow, void* window)
    : title(title), width(width), height(height), debug(debug), html("") {
    if (createWindow) {
        webview_ = std::make_unique<webview::webview>(debug, window);
        webview_->set_title(title.c_str());
        webview_->set_size(width, height, WEBVIEW_HINT_NONE);
        FABRIC_LOG_INFO("WebView created: {} ({}x{})", title, width, height);
    }
}

void WebView::setTitle(const std::string& title) {
    this->title = title;
    if (webview_) {
        webview_->set_title(title.c_str());
    }
}

void WebView::setSize(int width, int height, webview_hint_t hint) {
    this->width = width;
    this->height = height;
    if (webview_) {
        webview_->set_size(width, height, hint);
    }
}

void WebView::navigate(const std::string& url) {
    if (webview_) {
        webview_->navigate(url.c_str());
        FABRIC_LOG_INFO("WebView navigating to: {}", url);
    }
}

void WebView::setHTML(const std::string& html) {
    this->html = html;
    if (webview_) {
        webview_->set_html(html.c_str());
        FABRIC_LOG_DEBUG("WebView HTML content set");
    }
}

void WebView::run() {
    if (webview_) {
        FABRIC_LOG_INFO("Starting WebView main loop");
        webview_->run();
    } else {
        FABRIC_LOG_WARN("Attempting to run a WebView that was not created");
    }
}

void WebView::terminate() {
    if (webview_) {
        FABRIC_LOG_INFO("Terminating WebView");
        webview_->terminate();
    }
}

void WebView::eval(const std::string& js) {
    if (webview_) {
        webview_->eval(js.c_str());
    }
}

void WebView::bind(const std::string& name, const std::function<std::string(const std::string&)>& fn) {
    if (webview_) {
        webview_->bind(name.c_str(), [fn](const std::string& req) -> std::string { return fn(req); });
        FABRIC_LOG_DEBUG("Bound JavaScript function: {}", name);
    }
}

} // namespace fabric

#endif // FABRIC_USE_WEBVIEW

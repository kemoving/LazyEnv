// LazyEnv - Cross-platform, recoverable, zero-pollution dev environment configurator
// Copyright (C) 2026 Rein
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once
// ============================================================================
// LazyEnv - webview_host.h
// WebView2 host window management
// ============================================================================

#ifndef LAZYENV_WEBVIEW_HOST_H
#define LAZYENV_WEBVIEW_HOST_H

#include <Windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <string>
#include <functional>
#include <memory>

namespace lazyenv {

// Custom window messages for cross-thread communication
constexpr UINT WM_WEBVIEW_POST_MESSAGE = WM_APP + 1;
constexpr UINT WM_WEBVIEW_DRAG_START   = WM_APP + 2;

// Message handler: receives JSON string from web, returns JSON response.
using MessageHandler = std::function<std::string(const std::string& message)>;

// ----------------------------------------------------------------------------
// WebViewHost
// Manages a single WebView2 instance embedded in a Win32 window.
// Thread-safe postMessage via Win32 message queue.
// ----------------------------------------------------------------------------
class WebViewHost {
public:
    WebViewHost();
    ~WebViewHost();

    // Initialize WebView2 in the given parent window.
    // Navigates to the given file:// URI to load the HTML page.
    // Returns true if initialization started successfully (async).
    bool initialize(HWND parentWindow, const std::wstring& navigateUri);

    // Register a handler for messages from the web layer.
    void setMessageHandler(MessageHandler handler);

    // Post a JSON message from native to web.
    // THREAD-SAFE: can be called from any thread. Internally marshals
    // to the UI thread via PostMessage + WM_WEBVIEW_POST_MESSAGE.
    void postMessage(const std::string& json);

    // Resize the WebView to fill the parent window.
    void resize();

    // Deliver a JSON message directly to the web layer.
    // MUST be called on the UI thread only (called from WndProc).
    void deliverMessage(const std::string& json);

    // Check if WebView2 runtime is installed.
    static bool isRuntimeAvailable();

    // Get the underlying controller (for advanced use).
    ICoreWebView2Controller* getController() const;

    // Get the underlying webview (for direct PostWebMessageAsString).
    ICoreWebView2* getWebView() const;

private:
    HWND                                          parentHwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2>           webview_;
    MessageHandler                                messageHandler_;
    std::wstring                                  pendingUri_;

    void onWebViewCreated(ICoreWebView2Controller* controller);
    void setupMessageBridge();
};

} // namespace lazyenv

#endif // LAZYENV_WEBVIEW_HOST_H

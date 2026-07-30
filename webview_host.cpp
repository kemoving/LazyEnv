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

// ============================================================================
// LazyEnv - webview_host.cpp
// WebView2 host window management implementation
//
// Key design changes:
// 1. Uses Navigate(file:// URI) instead of NavigateToString to load HTML.
//    This allows external CSS/JS files to be loaded naturally by the browser,
//    avoiding all inline replacement issues.
// 2. postMessage() is thread-safe via Win32 message queue.
// ============================================================================

#include "webview_host.h"

#include <ShlObj.h>

#include <filesystem>
#include <string>

namespace {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                  static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) {
        // Conversion failed (invalid UTF-8); fall back to ASCII-safe copy
        std::wstring ws;
        ws.reserve(s.size());
        for (unsigned char c : s) ws.push_back(static_cast<wchar_t>(c));
        return ws;
    }
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(),
                        static_cast<int>(s.size()), ws.data(), len);
    return ws;
}

std::string wideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(),
                                  static_cast<int>(ws.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        // Conversion failed; fall back to ASCII-safe copy
        std::string s;
        s.reserve(ws.size());
        for (wchar_t c : ws) s.push_back(static_cast<char>(c & 0x7F));
        return s;
    }
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(),
                        static_cast<int>(ws.size()),
                        s.data(), len, nullptr, nullptr);
    return s;
}

} // anonymous namespace

namespace lazyenv {

WebViewHost::WebViewHost() = default;
WebViewHost::~WebViewHost() = default;

// ---------------------------------------------------------------------------
// Runtime detection
// ---------------------------------------------------------------------------
bool WebViewHost::isRuntimeAvailable() {
    wchar_t* version = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    if (SUCCEEDED(hr) && version) {
        CoTaskMemFree(version);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------
bool WebViewHost::initialize(HWND parentWindow, const std::wstring& navigateUri) {
    parentHwnd_ = parentWindow;
    pendingUri_ = navigateUri;

    // User data folder under %LOCALAPPDATA%/LazyEnv/WebView2
    wchar_t* appdata = nullptr;
    std::wstring userDataFolder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appdata))) {
        userDataFolder = std::wstring(appdata) + L"\\LazyEnv\\WebView2";
        CoTaskMemFree(appdata);
    }

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.c_str(),
        nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) return result;

                env->CreateCoreWebView2Controller(
                    parentHwnd_,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (SUCCEEDED(result) && controller) {
                                onWebViewCreated(controller);
                            }
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    return SUCCEEDED(hr);
}

// ---------------------------------------------------------------------------
// WebView created callback
// ---------------------------------------------------------------------------
void WebViewHost::onWebViewCreated(ICoreWebView2Controller* controller) {
    controller_ = controller;
    controller_->get_CoreWebView2(&webview_);

    // Configure settings
    wil::com_ptr<ICoreWebView2Settings> settings;
    webview_->get_Settings(&settings);
    settings->put_IsScriptEnabled(TRUE);
    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    settings->put_IsWebMessageEnabled(TRUE);
    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_AreDefaultContextMenusEnabled(FALSE);

    // Make WebView2 background transparent to match borderless window
    wil::com_ptr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(controller_->QueryInterface(IID_PPV_ARGS(&controller2)))) {
        COREWEBVIEW2_COLOR bg = { 255, 25, 25, 25 };
        controller2->put_DefaultBackgroundColor(bg);
    }

    // Setup message bridge
    setupMessageBridge();

    // Resize to fill parent
    resize();

    // Navigate to the local HTML file via file:// URI
    webview_->Navigate(pendingUri_.c_str());
    pendingUri_.clear();
}

// ---------------------------------------------------------------------------
// Message bridge: web -> native
// ---------------------------------------------------------------------------
void WebViewHost::setupMessageBridge() {
    if (!webview_) return;

    webview_->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this]([[maybe_unused]] ICoreWebView2* sender,
                   ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                wchar_t* rawMsg = nullptr;
                args->TryGetWebMessageAsString(&rawMsg);
                if (rawMsg) {
                    std::string msg = wideToUtf8(rawMsg);
                    CoTaskMemFree(rawMsg);

                    if (messageHandler_) {
                        try {
                            std::string response = messageHandler_(msg);
                            if (!response.empty()) {
                                // This callback runs on the UI thread, so direct
                                // delivery is safe here.
                                std::wstring wjson = utf8ToWide(response);
                                webview_->PostWebMessageAsString(wjson.c_str());
                            }
                        } catch (const std::exception& e) {
                            // Prevent any unhandled C++ exception from
                            // escaping the WebView2 callback boundary,
                            // which would cause STATUS_FATAL_USER_CALLBACK_EXCEPTION
                            // (0xC000041D) and kill the process silently.
                            std::string errMsg = "{\"action\":\"error\",\"message\":\""
                                + std::string(e.what()) + "\"}";
                            std::wstring wjson = utf8ToWide(errMsg);
                            webview_->PostWebMessageAsString(wjson.c_str());
                        } catch (...) {
                            std::string errMsg = "{\"action\":\"error\","
                                "\"message\":\"Unknown internal error\"}";
                            std::wstring wjson = utf8ToWide(errMsg);
                            webview_->PostWebMessageAsString(wjson.c_str());
                        }
                    }
                }
                return S_OK;
            }).Get(),
        nullptr);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void WebViewHost::setMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

// ---------------------------------------------------------------------------
// postMessage - THREAD-SAFE
//
// Instead of a shared queue + mutex (which crashes when the mutex internal
// state is corrupted by SEH stack damage), we PostMessageW a heap-allocated
// std::string* directly. Windows' message queue is inherently thread-safe.
// The UI thread receives WM_WEBVIEW_POST_MESSAGE, converts to wstring,
// calls PostWebMessageAsString, and deletes the pointer.
// ---------------------------------------------------------------------------
void WebViewHost::postMessage(const std::string& json) {
    if (!parentHwnd_) return;

    auto* msg = new std::string(json);
    if (!PostMessageW(parentHwnd_, WM_WEBVIEW_POST_MESSAGE, 0,
                      reinterpret_cast<LPARAM>(msg))) {
        // Window may have been destroyed between the check and PostMessageW
        delete msg;
    }
}

void WebViewHost::resize() {
    if (!controller_ || !parentHwnd_) return;
    RECT bounds;
    GetClientRect(parentHwnd_, &bounds);
    controller_->put_Bounds(bounds);
}

void WebViewHost::deliverMessage(const std::string& json) {
    if (!webview_) return;
    std::wstring wjson = utf8ToWide(json);
    webview_->PostWebMessageAsString(wjson.c_str());
}

ICoreWebView2Controller* WebViewHost::getController() const {
    return controller_.Get();
}

ICoreWebView2* WebViewHost::getWebView() const {
    return webview_.Get();
}

} // namespace lazyenv

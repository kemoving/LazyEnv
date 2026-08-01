// =============================================================================
// LazyEnv — Diagnostic Logging (header-only)
//
// Usage:
//   DBG_LOG("WM_SIZE: restored, client=" << cr.right << "x" << cr.bottom);
//
// Output:
//   1. %TEMP%\LazyEnv_debug.log  (timestamped, append)
//   2. OutputDebugStringA        (visible in DebugView / VS Output)
//
// Thread-safe via a slim critical section.
// =============================================================================
#pragma once

#include <Windows.h>
#include <chrono>
#include <fstream>
#include <format>
#include <string>

namespace lazyenv::debug {

// RAII helper to avoid global-ctor races and guarantee flush on exit.
class Logger {
public:
    static Logger& instance() {
        static Logger s;  // C++11 magic statics are thread-safe
        return s;
    }

    void write(const std::string& msg) {
        EnterCriticalSection(&cs_);
        if (!initialized_) initInternal();
        auto now    = std::chrono::system_clock::now();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()) %
                      1000;
        auto tt = std::chrono::system_clock::to_time_t(now);
        tm local{};
        localtime_s(&local, &tt);

        char buf[128];
        strftime(buf, sizeof(buf), "%H:%M:%S", &local);
        std::string line = std::format("[{}.{:03}] {}\n",
                                       buf, millis.count(), msg);

        // File
        if (file_.is_open()) {
            file_ << line;
            file_.flush();  // every line — data loss is the enemy of debugging
        }

        // OutputDebugString
        OutputDebugStringA(line.c_str());

        LeaveCriticalSection(&cs_);
    }

private:
    Logger() { InitializeCriticalSection(&cs_); }
    ~Logger() {
        if (file_.is_open()) file_.close();
        DeleteCriticalSection(&cs_);
    }

    void initInternal() {
        initialized_ = true;
        wchar_t tmp[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tmp)) {
            std::wstring path = std::wstring(tmp) + L"LazyEnv_debug.log";
            file_.open(path, std::ios::app);
            if (file_.is_open()) {
                std::string header = std::format(
                    "\n=== LazyEnv Debug Log (pid={}) ===\n",
                    GetCurrentProcessId());
                file_ << header;
                OutputDebugStringA(header.c_str());
            }
        }
    }

    CRITICAL_SECTION cs_;
    bool             initialized_ = false;
    std::ofstream    file_;
};

} // namespace lazyenv::debug

// Convenience macro — appends timestamp + newline automatically
#define DBG_LOG(expr)                                             \
    do {                                                          \
        try {                                                     \
            lazyenv::debug::Logger::instance().write(             \
                std::format("{}", expr));                         \
        } catch (...) {}                                          \
    } while (0)

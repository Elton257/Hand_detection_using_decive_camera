// ============================================================================
//  SharedState.hpp - state shared between the window (UI thread) and the
//  Worker thread. Replaces all former global variables.
//
//  Simple flags are atomics. The preview image and the status text are
//  guarded by a mutex; the window reads them with read().
// ============================================================================
#pragma once
#include "Config.hpp"
#include "WinCommon.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

constexpr UINT WM_APP_FRAME = WM_APP + 1;   // new frame ready to be drawn
constexpr UINT WM_APP_SIZE  = WM_APP + 2;   // preview size (wParam=w, lParam=h)

class SharedState {
public:
    // --- flags (lock-free) ---
    std::atomic<bool> running{true};              // false = everything should stop
    std::atomic<bool> active{false};              // hand control on/off (Ctrl+Alt+F8)
    std::atomic<bool> swapHands{cfg::SWAP_HANDS_DEFAULT};
    std::atomic<bool> snapshotRequest{false};     // save a camera snapshot (Ctrl+Alt+F11)
    std::atomic<HWND> hwnd{nullptr};

    // --- pen / eraser icon positions on screen ---
    void setPen(POINT p)    { penX_ = p.x; penY_ = p.y; hasPen_ = true; }
    void setEraser(POINT p) { erX_ = p.x;  erY_ = p.y;  hasEr_ = true; }
    bool hasPen() const     { return hasPen_; }
    bool hasEraser() const  { return hasEr_; }
    POINT pen() const       { return POINT{penX_.load(), penY_.load()}; }
    POINT eraser() const    { return POINT{erX_.load(), erY_.load()}; }

    // --- preview + text (mutex) ---
    void resizePreview(int w, int h);                       // allocates only here
    void publish(const uint32_t* bgra, size_t count,        // new preview + 3 status lines
                 const wchar_t* l0, const wchar_t* l1, const wchar_t* l2);
    void setStatus(const wchar_t* l0);                      // single message, clears lines 1-2
    void setMessage(const wchar_t* text, DWORD ms = 3000);  // temporary bottom message

    // Calls f(img, w, h, lines, msgOrEmpty) while holding the lock.
    template <class F>
    void read(F&& f) {
        std::lock_guard<std::mutex> lk(m_);
        const wchar_t* msg = GetTickCount64() < msgUntil_ ? msg_ : L"";
        f(img_, w_, h_, lines_, msg);
    }

    // Sends a message to the window (safe from any thread).
    void post(UINT msg, WPARAM w = 0, LPARAM l = 0) const {
        HWND h = hwnd.load();
        if (h) PostMessageW(h, msg, w, l);
    }

private:
    std::atomic<bool> hasPen_{false}, hasEr_{false};
    std::atomic<LONG> penX_{0}, penY_{0}, erX_{0}, erY_{0};

    std::mutex m_;
    std::vector<uint32_t> img_;
    int w_ = 0, h_ = 0;
    wchar_t lines_[3][160] = {};
    wchar_t msg_[160] = {};
    ULONGLONG msgUntil_ = 0;
};

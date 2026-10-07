// ============================================================================
//  MainWindow.hpp - the small preview window (always on top, never takes focus)
//  and the global Ctrl+Alt+F-key shortcuts
// ============================================================================
#pragma once
#include "SharedState.hpp"
#include "WinCommon.hpp"

class MainWindow {
public:
    explicit MainWindow(SharedState& state) : state_(state) {}
    ~MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    // Creates the window and registers the shortcuts. false on failure.
    bool create(HINSTANCE hInst);
    HWND handle() const { return hwnd_; }

private:
    // Win32 calls this static function; it forwards to the right object.
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);

    void registerHotkeys();
    void unregisterHotkeys();
    void onHotKey(int id);
    void onPaint();
    void fitToPreview(int pw, int ph);
    void freeBackBuffer();
    static void textShadow(HDC dc, int x, int y, const wchar_t* s, COLORREF c);

    SharedState& state_;
    HWND hwnd_ = nullptr;
    // Off-screen buffer for flicker-free drawing (recreated only when the size changes)
    HDC hdcMem_ = nullptr;
    HBITMAP bmp_ = nullptr, oldBmp_ = nullptr;
    HFONT font_ = nullptr;
    int memW_ = 0, memH_ = 0;
};

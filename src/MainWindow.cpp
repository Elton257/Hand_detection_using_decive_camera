#include "MainWindow.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cwchar>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

namespace {
enum { HK_TOGGLE = 1, HK_PEN, HK_ERASER, HK_SWAP, HK_SNAP };

// Ctrl+Alt + F-keys. CAUTION: never use letters/digits with Ctrl+Alt!
// On Windows the AltGr key = Ctrl+Alt, so Ctrl+Alt+Q would be caught every time
// you type e.g. '@' or a backslash with AltGr+Q (and the program used to close).
// F-keys don't type any character on any keyboard layout.
struct HotKey { int id; UINT vk; const wchar_t* name; };
const HotKey kKeys[] = {
    {HK_TOGGLE, VK_F8,  L"F8"},  {HK_PEN,  VK_F9,  L"F9"}, {HK_ERASER, VK_F10, L"F10"},
    {HK_SNAP,   VK_F11, L"F11"}, {HK_SWAP, VK_F7,  L"F7"}};
}  // namespace

MainWindow::~MainWindow() {
    unregisterHotkeys();
    freeBackBuffer();
    if (font_) DeleteObject(font_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool MainWindow::create(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &MainWindow::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.lpszClassName = L"HandBoardWnd";
    RegisterClassExW(&wc);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const DWORD ex = WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_APPWINDOW;
    RECT r = {0, 0, 480, 360};
    AdjustWindowRectEx(&r, style, FALSE, ex);
    // 'this' travels in lpParam and is stored on WM_NCCREATE
    hwnd_ = CreateWindowExW(ex, wc.lpszClassName, L"HandBoard – shkruaj me dorë", style,
                            CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                            nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;
    state_.hwnd = hwnd_;

    registerHotkeys();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        reinterpret_cast<MainWindow*>(cs->lpCreateParams)->hwnd_ = hwnd;
    }
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT MainWindow::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        font_ = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, L"Segoe UI");
        return 0;
    case WM_APP_FRAME:
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_APP_SIZE:
        fitToPreview((int)wp, (int)lp);
        return 0;
    case WM_HOTKEY:
        onHotKey((int)wp);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        onPaint();
        return 0;
    case WM_CLOSE:
        Logger::write(L"Mbyllje: dritarja u mbyll (butoni X ose Alt+F4)");
        break;                                        // DefWindowProc -> DestroyWindow
    case WM_ENDSESSION:
        if (wp) Logger::write(L"Mbyllje: Windows po fiket / ristartohet / del perdoruesi");
        return 0;
    case WM_DESTROY:
        state_.running = false;
        state_.hwnd = nullptr;
        unregisterHotkeys();
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);   // no more messages to this object
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

void MainWindow::registerHotkeys() {
    const UINT mods = MOD_CONTROL | MOD_ALT | 0x4000 /* MOD_NOREPEAT */;
    int failed = 0;
    for (const HotKey& k : kKeys)
        if (!RegisterHotKey(hwnd_, k.id, mods, k.vk)) {
            ++failed;
            Logger::write(L"Shkurtesa Ctrl+Alt+%ls eshte e zene nga nje program tjeter", k.name);
        }
    if (failed) state_.setMessage(L"Disa shkurtesa Ctrl+Alt+F janë të zëna (shih HandBoard.log)", 6000);
    else        state_.setMessage(L"Hap whiteboard-in, zgjidh lapsin, pastaj Ctrl+Alt+F8", 8000);
}

void MainWindow::unregisterHotkeys() {
    if (!hwnd_) return;
    for (const HotKey& k : kKeys) UnregisterHotKey(hwnd_, k.id);
}

void MainWindow::onHotKey(int id) {
    POINT p;
    GetCursorPos(&p);
    switch (id) {
    case HK_TOGGLE: {
        const bool a = !state_.active.load();
        state_.active = a;
        state_.setMessage(a ? L"Kontrolli me dorë: AKTIV" : L"Kontrolli me dorë: NDALUR");
        break;
    }
    case HK_PEN:    state_.setPen(p);    state_.setMessage(L"Pozicioni i lapsit u ruajt ✓"); break;
    case HK_ERASER: state_.setEraser(p); state_.setMessage(L"Pozicioni i gomës u ruajt ✓"); break;
    case HK_SWAP: {
        const bool s = !state_.swapHands.load();
        state_.swapHands = s;
        state_.setMessage(s ? L"Duart u ndërruan (mëngjarash)" : L"Duart normale");
        break;
    }
    case HK_SNAP:   state_.snapshotRequest = true; break;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::freeBackBuffer() {
    if (hdcMem_) {
        SelectObject(hdcMem_, oldBmp_);
        DeleteObject(bmp_);
        DeleteDC(hdcMem_);
        hdcMem_ = nullptr;
        bmp_ = oldBmp_ = nullptr;
    }
}

void MainWindow::textShadow(HDC dc, int x, int y, const wchar_t* s, COLORREF c) {
    const int n = (int)wcslen(s);
    SetTextColor(dc, RGB(0, 0, 0));
    TextOutW(dc, x + 1, y + 1, s, n);
    SetTextColor(dc, c);
    TextOutW(dc, x, y, s, n);
}

void MainWindow::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    RECT rc;
    GetClientRect(hwnd_, &rc);
    const int cw = std::max(1, (int)rc.right), ch = std::max(1, (int)rc.bottom);
    if (!hdcMem_ || memW_ != cw || memH_ != ch) {
        freeBackBuffer();
        hdcMem_ = CreateCompatibleDC(hdc);
        bmp_ = CreateCompatibleBitmap(hdc, cw, ch);
        oldBmp_ = (HBITMAP)SelectObject(hdcMem_, bmp_);
        memW_ = cw;
        memH_ = ch;
    }

    wchar_t l0[160], l1[160], l2[160], m[160];
    state_.read([&](const auto& img, int w, int h, const auto& lines, const wchar_t* msg) {
        if (!img.empty() && w > 0 && h > 0) {
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -h;   // top-down
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            SetStretchBltMode(hdcMem_, COLORONCOLOR);
            StretchDIBits(hdcMem_, 0, 0, cw, ch, 0, 0, w, h, img.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
        } else {
            RECT f = {0, 0, cw, ch};
            FillRect(hdcMem_, &f, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }
        wcscpy(l0, lines[0]);
        wcscpy(l1, lines[1]);
        wcscpy(l2, lines[2]);
        wcsncpy(m, msg, 159);
        m[159] = 0;
    });

    HFONT of = (HFONT)SelectObject(hdcMem_, font_);
    SetBkMode(hdcMem_, TRANSPARENT);
    textShadow(hdcMem_, 8, 6, l0, state_.active.load() ? RGB(90, 255, 120) : RGB(255, 210, 80));
    textShadow(hdcMem_, 8, 26, l1, RGB(255, 170, 90));
    textShadow(hdcMem_, 8, 46, l2, RGB(120, 190, 255));
    if (m[0]) textShadow(hdcMem_, 8, ch - 44, m, RGB(255, 255, 255));
    textShadow(hdcMem_, 8, ch - 22, L"Ctrl+Alt:  F8 nis/ndal · F9 laps · F10 gomë · F11 foto · F7 duart", RGB(210, 210, 210));
    SelectObject(hdcMem_, of);
    BitBlt(hdc, 0, 0, cw, ch, hdcMem_, 0, 0, SRCCOPY);
    EndPaint(hwnd_, &ps);
}

// Place the window bottom-right, 1.5x the preview size.
void MainWindow::fitToPreview(int pw, int ph) {
    RECT r = {0, 0, pw * 3 / 2, ph * 3 / 2};
    const DWORD style = (DWORD)GetWindowLongPtrW(hwnd_, GWL_STYLE);
    const DWORD ex = (DWORD)GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    AdjustWindowRectEx(&r, style, FALSE, ex);
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    const int ww = r.right - r.left, wh = r.bottom - r.top;
    SetWindowPos(hwnd_, HWND_TOPMOST, wa.right - ww - 12, wa.bottom - wh - 12, ww, wh, SWP_NOACTIVATE);
}

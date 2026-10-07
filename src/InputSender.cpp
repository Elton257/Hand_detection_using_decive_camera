#include "InputSender.hpp"

#include <algorithm>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#endif

void InputSender::refresh() {
    sw = std::max(2, GetSystemMetrics(SM_CXSCREEN));
    sh = std::max(2, GetSystemMetrics(SM_CYSCREEN));
}

void InputSender::move(int x, int y) const {
    x = std::min(std::max(x, 0), sw - 1);
    y = std::min(std::max(y, 0), sh - 1);
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dx = MulDiv(x, 65535, sw - 1);
    in.mi.dy = MulDiv(y, 65535, sh - 1);
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &in, sizeof(INPUT));
}

void InputSender::press() {
    if (down) return;
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &in, sizeof(INPUT));
    down = true;
}

void InputSender::release() {
    if (!down) return;
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &in, sizeof(INPUT));
    down = false;
}

void InputSender::click() {
    release();
    INPUT in[2] = {};
    in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, in, sizeof(INPUT));
}

void InputSender::ctrlWheel(int delta) const {
    INPUT in[3] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
    in[1].type = INPUT_MOUSE;    in[1].mi.dwFlags = MOUSEEVENTF_WHEEL; in[1].mi.mouseData = (DWORD)delta;
    in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = VK_CONTROL; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(3, in, sizeof(INPUT));
}

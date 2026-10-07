// ============================================================================
//  InputSender.hpp - virtual mouse and keyboard (SendInput)
// ============================================================================
#pragma once
#include "WinCommon.hpp"

class InputSender {
public:
    int sw = 1920, sh = 1080;   // primary screen size in pixels
    bool down = false;          // whether the left button is held down

    void refresh();                    // re-reads the screen size
    void move(int x, int y) const;     // moves the cursor (absolute coordinates)
    void press();                      // presses the left button (if not already down)
    void release();                    // releases the left button (if down)
    void click();                      // full click
    void ctrlWheel(int delta) const;   // Ctrl + mouse wheel (zoom)
};

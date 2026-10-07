// ============================================================================
//  Preview.hpp - small preview: camera + hand skeleton (with OpenCV)
// ============================================================================
#pragma once
#include "Gestures.hpp"
#include "HandDetector.hpp"

#include <opencv2/core.hpp>

#include <cstdint>

// What to draw on top of the camera image.
struct PreviewInfo {
    int nHands = 0;
    const HandResult* hands = nullptr;
    int rightIdx = -1, leftIdx = -1;     // which hand is which
    bool tipOk = false;                  // whether there is a fingertip this frame
    float tipX = 0, tipY = 0;            // in camera image pixels
    RState state = RState::None;
};

class PreviewRenderer {
public:
    // frame: BGR (mirrored). out: pw*ph BGRA pixels for GDI (written in place).
    void render(const cv::Mat& frame, const PreviewInfo& info, int pw, int ph, uint32_t* out);

private:
    cv::Mat work_;                       // reused between frames
};

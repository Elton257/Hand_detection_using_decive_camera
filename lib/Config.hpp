// ============================================================================
//  Config.hpp - all program settings in one place
// ============================================================================
#pragma once

namespace cfg {
// Models (in the "models" folder next to HandBoard.exe)
constexpr const wchar_t* PALM_MODEL = L"models\\palm_detection_mediapipe_2023feb.onnx";
constexpr const wchar_t* HAND_MODEL = L"models\\handpose_estimation_mediapipe_2023feb.onnx";
constexpr float PALM_SCORE = 0.6f;             // minimum confidence of the palm detector
constexpr float HAND_CONF  = 0.8f;             // minimum confidence of the 21 hand landmarks
constexpr int   DNN_THREADS = 4;               // CPU threads OpenCV may use (don't hog the whole CPU during a meeting)

constexpr int PREVIEW_W = 320;                 // width of the small preview window

// Part of the camera image (0..1) that maps to the WHOLE screen.
// Smaller = smaller hand movements, but less precision.
constexpr float AREA_X0 = 0.12f, AREA_X1 = 0.88f;
constexpr float AREA_Y0 = 0.08f, AREA_Y1 = 0.72f;

// Gestures
constexpr float FINGER_STRAIGHT_DEG = 140.0f;  // straight finger: angle at the middle joint (PIP) above this
constexpr float THUMB_OUT_RATIO     = 0.60f;   // thumb out: tip far from the index base (x palm length)

constexpr int      FRAMES_TO_PRESS   = 3;      // consecutive frames needed to START drawing/erasing
constexpr int      FRAMES_TO_RELEASE = 2;      // consecutive frames needed to STOP
constexpr int      FRAMES_ZOOM       = 4;
constexpr unsigned ZOOM_INTERVAL_MS  = 140;    // how often one zoom step is sent

constexpr float EURO_MINCUT = 1.2f;            // smoothing when the hand is still (lower = steadier)
constexpr float EURO_BETA   = 0.006f;          // responsiveness when the hand moves fast (higher = less lag)
constexpr float EURO_DCUT   = 1.0f;

constexpr float MAX_JUMP = 0.20f;              // max jump (fraction of the screen) while drawing

constexpr bool SWAP_HANDS_DEFAULT = false;
}  // namespace cfg

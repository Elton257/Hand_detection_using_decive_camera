// ============================================================================
//  Worker.hpp - the worker thread: camera -> hands -> gestures -> mouse
// ============================================================================
#pragma once
#include "Camera.hpp"
#include "Gestures.hpp"
#include "HandDetector.hpp"
#include "InputSender.hpp"
#include "Preview.hpp"
#include "SharedState.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <vector>

class Worker {
public:
    explicit Worker(SharedState& state) : state_(state) {}
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    // Thread body. Runs until state.running becomes false.
    // Releases the mouse and the camera before returning.
    void run();

private:
    // One processed frame: the result of the steps below.
    struct FrameResult {
        int nHands = 0;
        int rightIdx = -1, leftIdx = -1;
        RState right = RState::None;     // debounced
        LState left = LState::None;      // debounced
        bool hasTip = false;
        float tipX = 0, tipY = 0;        // camera pixels
        int screenX = -1, screenY = -1;  // filtered screen point
    };

    bool loadModels();
    bool openCamera();                   // allocates frame/preview buffers (only here)
    void processFrame();
    void handleSnapshot();
    void detectHands(FrameResult& r);
    void assignHands(FrameResult& r) const;
    void readGestures(FrameResult& r, float dt);
    void applyActions(FrameResult& r);
    void publish(const FrameResult& r);
    void switchTool(bool toEraser, int retX, int retY);
    bool saveSnapshot(wchar_t* outName, size_t outLen) const;
    static const wchar_t* rightText(RState st, bool eraserTool);

    SharedState& state_;

    // --- components ---
    HandDetector detector_;
    Camera camera_;
    InputSender mouse_;
    PreviewRenderer renderer_;
    OneEuro filterX_, filterY_;
    Debounce<RState> rightDeb_{RState::None};
    Debounce<LState> leftDeb_{LState::None};

    // --- buffers (allocated when the camera opens, reused every frame) ---
    std::vector<uint32_t> frame_, preview_;
    cv::Mat bgr_, mirrored_;
    HandResult hands_[2];
    int previewW_ = 0, previewH_ = 0;

    // --- state between frames ---
    bool cameraOk_ = false;
    bool cameraFailLogged_ = false;
    bool eraserTool_ = false;            // eraser currently selected in the whiteboard
    int lostFrames_ = 999;
    int glitches_ = 0;
    int sentX_ = -1, sentY_ = -1;        // last point sent to the mouse
    double lastTime_ = 0, fpsTime_ = 0, lastScreenCheck_ = 0;
    int fpsFrames_ = 0;
    float fps_ = 0;
    ULONGLONG lastZoom_ = 0;
    ULONGLONG lastDnnErrorLog_ = 0;
};

// ============================================================================
//  HandDetector.hpp - hand detection with OpenCV DNN + MediaPipe models
//
//  Two neural networks from OpenCV Zoo (Apache 2.0 license):
//    1. palm_detection_mediapipe_2023feb.onnx       finds palms (192x192)
//    2. handpose_estimation_mediapipe_2023feb.onnx  21 landmarks per hand + handedness (224x224)
//
//  Like MediaPipe: once a hand is found, the next frame searches directly from
//  its landmarks (skipping the palm detector), which is much faster.
//
//  The pre/post-processing code is taken from OpenCV Zoo (mp_palmdet.py,
//  mp_handpose.py, demo.cpp) and translated to C++.
// ============================================================================
#pragma once
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include <string>
#include <vector>

// Indices of the 21 landmarks (MediaPipe)
enum Lm {
    WRIST = 0,
    THUMB_CMC = 1, THUMB_MCP = 2, THUMB_IP = 3, THUMB_TIP = 4,
    INDEX_MCP = 5, INDEX_PIP = 6, INDEX_DIP = 7, INDEX_TIP = 8,
    MIDDLE_MCP = 9, MIDDLE_PIP = 10, MIDDLE_DIP = 11, MIDDLE_TIP = 12,
    RING_MCP = 13, RING_PIP = 14, RING_DIP = 15, RING_TIP = 16,
    PINKY_MCP = 17, PINKY_PIP = 18, PINKY_DIP = 19, PINKY_TIP = 20
};

struct HandResult {
    cv::Point2f pts[21];      // coordinates in the given image (pixels)
    float handedness = 0.f;   // > 0.5 = right hand
    float conf = 0.f;         // confidence 0..1
    bool isRight() const { return handedness > 0.5f; }
};

struct Palm {
    float x1, y1, x2, y2;     // palm bounding box
    cv::Point2f lm[7];        // 7 points: wrist, finger bases, thumb
    float score;
};

class HandDetector {
public:
    // Loads the models. Returns false + err if they are missing or unreadable.
    bool load(const std::string& palmModel, const std::string& handModel, std::string& err);

    // Finds up to 2 hands in the BGR image. Returns the number of hands.
    int detect(const cv::Mat& bgr, HandResult out[2]);

    // Individual steps (public for testing)
    std::vector<Palm> detectPalms(const cv::Mat& bgr);
    bool estimate(const cv::Mat& bgr, const Palm& palm, HandResult& out);

    float palmScoreThreshold = 0.6f;
    float handConfThreshold = 0.8f;

private:
    cv::dnn::Net palmNet_, handNet_;
    std::vector<cv::Point2f> anchors_;
    // tracking
    HandResult last_[2];
    int nLast_ = 0;
    int frame_ = 0;
    // reused buffers (OpenCV reallocates only when the size changes)
    cv::Mat resized_, padded_, rgb_, f32_, crop1_, rot_, crop2_, in224_, f224_;

    void makeAnchors();
};

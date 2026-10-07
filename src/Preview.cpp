#include "Preview.hpp"
#include "Config.hpp"

#include <opencv2/imgproc.hpp>

namespace {
const int kBones[21][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 4},                // thumb
    {0, 5}, {5, 6}, {6, 7}, {7, 8},                // index
    {5, 9}, {9, 10}, {10, 11}, {11, 12},           // middle
    {9, 13}, {13, 14}, {14, 15}, {15, 16},         // ring
    {13, 17}, {17, 18}, {18, 19}, {19, 20}, {0, 17} // pinky
};
}

void PreviewRenderer::render(const cv::Mat& frame, const PreviewInfo& info, int pw, int ph, uint32_t* out) {
    cv::Mat& work = work_;
    cv::resize(frame, work, cv::Size(pw, ph), 0, 0, cv::INTER_AREA);
    work.convertTo(work, -1, 0.75, 0);                                  // slightly darker so the skeleton stands out
    const float sx = (float)pw / frame.cols, sy = (float)ph / frame.rows;
    auto P = [&](const cv::Point2f& p) { return cv::Point(cvRound(p.x * sx), cvRound(p.y * sy)); };

    cv::rectangle(work, cv::Point((int)(cfg::AREA_X0 * pw), (int)(cfg::AREA_Y0 * ph)),
                  cv::Point((int)(cfg::AREA_X1 * pw), (int)(cfg::AREA_Y1 * ph)), cv::Scalar(64, 208, 255), 1);
    for (int k = 0; k < info.nHands; ++k) {
        const HandResult& h = info.hands[k];
        const cv::Scalar col = (k == info.rightIdx) ? cv::Scalar(26, 140, 255)      // orange
                             : (k == info.leftIdx)  ? cv::Scalar(255, 160, 58)      // blue
                                                    : cv::Scalar(160, 160, 160);
        for (const auto& b : kBones) cv::line(work, P(h.pts[b[0]]), P(h.pts[b[1]]), col, 1, cv::LINE_AA);
        for (int j = 0; j < 21; ++j) cv::circle(work, P(h.pts[j]), 2, col, -1, cv::LINE_AA);
    }
    if (info.tipOk) {
        const cv::Scalar c = (info.state == RState::Draw)  ? cv::Scalar(48, 48, 255)
                           : (info.state == RState::Erase) ? cv::Scalar(255, 255, 255)
                                                           : cv::Scalar(0, 255, 255);
        cv::circle(work, P(cv::Point2f(info.tipX, info.tipY)), 5, c, -1, cv::LINE_AA);
    }
    cv::Mat dst(ph, pw, CV_8UC4, out);                                   // writes directly into the GDI buffer
    cv::cvtColor(work, dst, cv::COLOR_BGR2BGRA);
}

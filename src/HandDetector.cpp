#include "HandDetector.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kPalmSize = 192;
constexpr int kHandSize = 224;
// MediaPipe: palm landmarks used for tracking from the previous frame
const int kPalmLmIds[7] = {0, 5, 9, 13, 17, 1, 2};

// Builds an NHWC blob (1 x H x W x 3) from a continuous CV_32FC3 Mat.
cv::Mat NhwcBlob(cv::Mat& f32) {
    const int sz[4] = {1, f32.rows, f32.cols, 3};
    return cv::Mat(4, sz, CV_32F, f32.ptr<float>());
}

float BoxIoU(const cv::Rect2f& a, const cv::Rect2f& b) {
    const float inter = (a & b).area();
    const float uni = a.area() + b.area() - inter;
    return uni > 0.f ? inter / uni : 0.f;
}
}  // namespace

bool HandDetector::load(const std::string& palmModel, const std::string& handModel, std::string& err) {
    try {
        palmNet_ = cv::dnn::readNet(palmModel);
        handNet_ = cv::dnn::readNet(handModel);
    } catch (const cv::Exception& e) {
        err = e.what();
        return false;
    }
    if (palmNet_.empty() || handNet_.empty()) { err = "modeli bosh"; return false; }
    palmNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    palmNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    handNet_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    handNet_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    makeAnchors();
    return true;
}

// SSD anchors (MediaPipe): 24x24 with 2 per cell + 12x12 with 6 per cell = 2016.
// Same list OpenCV Zoo ships as a table (verified).
void HandDetector::makeAnchors() {
    anchors_.clear();
    const int fms[2] = {24, 12}, per[2] = {2, 6};
    for (int l = 0; l < 2; ++l)
        for (int y = 0; y < fms[l]; ++y)
            for (int x = 0; x < fms[l]; ++x)
                for (int k = 0; k < per[l]; ++k)
                    anchors_.emplace_back((x + 0.5f) / fms[l], (y + 0.5f) / fms[l]);
}

std::vector<Palm> HandDetector::detectPalms(const cv::Mat& bgr) {
    std::vector<Palm> palms;
    const int w = bgr.cols, h = bgr.rows;
    // --- preprocessing (mp_palmdet.py: _preprocess) ---
    const float ratio = std::min((float)kPalmSize / h, (float)kPalmSize / w);
    const int rw = (int)(w * ratio), rh = (int)(h * ratio);
    cv::resize(bgr, resized_, cv::Size(rw, rh));
    const int padW = kPalmSize - rw, padH = kPalmSize - rh;
    const int left = padW / 2, top = padH / 2;
    cv::copyMakeBorder(resized_, padded_, top, padH - top, left, padW - left, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    cv::cvtColor(padded_, rgb_, cv::COLOR_BGR2RGB);
    rgb_.convertTo(f32_, CV_32F, 1.0 / 255.0);
    const cv::Point2i padBias((int)(left / ratio), (int)(top / ratio));

    palmNet_.setInput(NhwcBlob(f32_));
    std::vector<cv::Mat> outs;
    palmNet_.forward(outs, std::vector<cv::String>{"Identity", "Identity_1"});
    const float* boxes = outs[0].ptr<float>();     // 2016 x 18
    const float* scores = outs[1].ptr<float>();    // 2016

    // --- postprocessing (mp_palmdet.py: _postprocess) ---
    const float scale = (float)std::max(w, h);
    std::vector<cv::Rect> rects;
    std::vector<float> sc;
    std::vector<int> idx;
    const int n = (int)anchors_.size();
    for (int i = 0; i < n; ++i) {
        const float s = 1.f / (1.f + std::exp(-scores[i]));
        if (s <= palmScoreThreshold) continue;
        const float* b = boxes + (size_t)i * 18;
        const cv::Point2f a = anchors_[i];
        const float cx = b[0] / kPalmSize, cy = b[1] / kPalmSize, bw = b[2] / kPalmSize, bh = b[3] / kPalmSize;
        Palm p;
        p.x1 = (cx - bw / 2 + a.x) * scale - padBias.x;
        p.y1 = (cy - bh / 2 + a.y) * scale - padBias.y;
        p.x2 = (cx + bw / 2 + a.x) * scale - padBias.x;
        p.y2 = (cy + bh / 2 + a.y) * scale - padBias.y;
        for (int j = 0; j < 7; ++j)
            p.lm[j] = cv::Point2f((b[4 + 2 * j] / kPalmSize + a.x) * scale - padBias.x,
                                  (b[5 + 2 * j] / kPalmSize + a.y) * scale - padBias.y);
        p.score = s;
        palms.push_back(p);
        rects.emplace_back((int)p.x1, (int)p.y1, (int)(p.x2 - p.x1), (int)(p.y2 - p.y1));
        sc.push_back(s);
    }
    std::vector<Palm> kept;
    if (palms.empty()) return kept;
    cv::dnn::NMSBoxes(rects, sc, palmScoreThreshold, 0.3f, idx);
    for (int i : idx) kept.push_back(palms[i]);
    return kept;
}

namespace {
// mp_handpose.py: _cropAndPadFromPalm
bool CropAndPad(const cv::Mat& img, float x1, float y1, float x2, float y2, bool forRotation,
                cv::Mat& out, cv::Vec4i& box, cv::Point2i& bias) {
    float w = x2 - x1, h = y2 - y1;
    if (!forRotation) { y1 += -0.4f * h; y2 += -0.4f * h; }        // PALM_BOX_SHIFT_VECTOR [0, -0.4]
    const float cx = (x1 + x2) / 2, cy = (y1 + y2) / 2;
    w = x2 - x1; h = y2 - y1;
    const float enl = forRotation ? 4.f : 3.f;                     // ENLARGE_FACTOR
    int bx1 = (int)(cx - w * enl / 2), by1 = (int)(cy - h * enl / 2);
    int bx2 = (int)(cx + w * enl / 2), by2 = (int)(cy + h * enl / 2);
    bx1 = std::min(std::max(bx1, 0), img.cols); bx2 = std::min(std::max(bx2, 0), img.cols);
    by1 = std::min(std::max(by1, 0), img.rows); by2 = std::min(std::max(by2, 0), img.rows);
    if (bx2 - bx1 < 2 || by2 - by1 < 2) return false;
    const cv::Mat crop = img(cv::Range(by1, by2), cv::Range(bx1, bx2));
    const int side = forRotation ? (int)std::sqrt((double)crop.rows * crop.rows + (double)crop.cols * crop.cols)
                                 : std::max(crop.rows, crop.cols);
    const int padH = side - crop.rows, padW = side - crop.cols;
    const int left = padW / 2, top = padH / 2;
    cv::copyMakeBorder(crop, out, top, padH - top, left, padW - left, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    box = cv::Vec4i(bx1, by1, bx2, by2);
    bias = cv::Point2i(bx1 - left, by1 - top);
    return true;
}
}  // namespace

bool HandDetector::estimate(const cv::Mat& bgr, const Palm& palm, HandResult& out) {
    // --- preprocessing (mp_handpose.py: _preprocess) ---
    cv::Vec4i box1;
    cv::Point2i padBias;
    if (!CropAndPad(bgr, palm.x1, palm.y1, palm.x2, palm.y2, true, crop1_, box1, padBias)) return false;
    cv::cvtColor(crop1_, crop1_, cv::COLOR_BGR2RGB);
    cv::Point2f lm[7];
    for (int j = 0; j < 7; ++j) lm[j] = palm.lm[j] - cv::Point2f((float)padBias.x, (float)padBias.y);
    const cv::Point2f p1 = lm[0], p2 = lm[2];                      // palm base, middle finger base
    double rad = CV_PI / 2 - std::atan2(-(p2.y - p1.y), p2.x - p1.x);
    rad = rad - 2 * CV_PI * std::floor((rad + CV_PI) / (2 * CV_PI));
    const double angle = rad * 180.0 / CV_PI;
    const cv::Point2f center((box1[0] + box1[2]) / 2.f - padBias.x, (box1[1] + box1[3]) / 2.f - padBias.y);
    const cv::Mat M = cv::getRotationMatrix2D(center, angle, 1.0);   // 2x3 CV_64F
    cv::warpAffine(crop1_, rot_, M, crop1_.size());
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (int j = 0; j < 7; ++j) {
        const double x = M.at<double>(0, 0) * lm[j].x + M.at<double>(0, 1) * lm[j].y + M.at<double>(0, 2);
        const double y = M.at<double>(1, 0) * lm[j].x + M.at<double>(1, 1) * lm[j].y + M.at<double>(1, 2);
        mnx = std::min(mnx, (float)x); mxx = std::max(mxx, (float)x);
        mny = std::min(mny, (float)y); mxy = std::max(mxy, (float)y);
    }
    cv::Vec4i box2;
    cv::Point2i unused;
    if (!CropAndPad(rot_, mnx, mny, mxx, mxy, false, crop2_, box2, unused)) return false;
    cv::resize(crop2_, in224_, cv::Size(kHandSize, kHandSize), 0, 0, cv::INTER_AREA);
    in224_.convertTo(f224_, CV_32F, 1.0 / 255.0);

    handNet_.setInput(NhwcBlob(f224_));
    std::vector<cv::Mat> outs;
    handNet_.forward(outs, std::vector<cv::String>{"Identity", "Identity_1", "Identity_2"});
    const float conf = outs[1].ptr<float>()[0];
    if (conf < handConfThreshold) return false;
    const float* L = outs[0].ptr<float>();                         // 21 x 3

    // --- postprocessing (mp_handpose.py: _postprocess) ---
    const float s = std::max((float)(box2[2] - box2[0]), (float)(box2[3] - box2[1])) / kHandSize;
    const cv::Mat R = cv::getRotationMatrix2D(cv::Point2f(0, 0), angle, 1.0);
    const double r00 = R.at<double>(0, 0), r01 = R.at<double>(0, 1), r10 = R.at<double>(1, 0), r11 = R.at<double>(1, 1);
    // inverse rotation of M
    const double m00 = M.at<double>(0, 0), m01 = M.at<double>(0, 1), m02 = M.at<double>(0, 2);
    const double m10 = M.at<double>(1, 0), m11 = M.at<double>(1, 1), m12 = M.at<double>(1, 2);
    const double i00 = m00, i01 = m10, i10 = m01, i11 = m11;      // transpose
    const double it0 = -(i00 * m02 + i01 * m12), it1 = -(i10 * m02 + i11 * m12);
    const double cx = (box2[0] + box2[2]) / 2.0, cy = (box2[1] + box2[3]) / 2.0;
    const double ocx = i00 * cx + i01 * cy + it0, ocy = i10 * cx + i11 * cy + it1;
    for (int k = 0; k < 21; ++k) {
        const double x = (L[3 * k] - kHandSize / 2.0) * s, y = (L[3 * k + 1] - kHandSize / 2.0) * s;
        // [x y] * R[:, :2]
        const double rx = x * r00 + y * r10, ry = x * r01 + y * r11;
        out.pts[k] = cv::Point2f((float)(rx + ocx + padBias.x), (float)(ry + ocy + padBias.y));
    }
    out.conf = conf;
    out.handedness = outs[2].ptr<float>()[0];
    return true;
}

int HandDetector::detect(const cv::Mat& bgr, HandResult out[2]) {
    int n = 0;
    ++frame_;
    // 1) tracking: palm from the previous frame's landmarks (like MediaPipe)
    for (int k = 0; k < nLast_ && n < 2; ++k) {
        Palm p;
        float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
        for (int j = 0; j < 7; ++j) {
            p.lm[j] = last_[k].pts[kPalmLmIds[j]];
            mnx = std::min(mnx, p.lm[j].x); mxx = std::max(mxx, p.lm[j].x);
            mny = std::min(mny, p.lm[j].y); mxy = std::max(mxy, p.lm[j].y);
        }
        p.x1 = mnx; p.y1 = mny; p.x2 = mxx; p.y2 = mxy; p.score = 1.f;
        if (estimate(bgr, p, out[n])) ++n;
    }
    // 2) palm detector: when no hand is tracked, or every 5 frames to find a new hand
    if (n < 2 && (n == 0 || frame_ % 5 == 0)) {
        const std::vector<Palm> palms = detectPalms(bgr);
        for (const Palm& p : palms) {
            if (n >= 2) break;
            const cv::Rect2f pr(p.x1, p.y1, p.x2 - p.x1, p.y2 - p.y1);
            bool dup = false;                                       // hand already being tracked?
            for (int k = 0; k < n && !dup; ++k) {
                float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
                for (const cv::Point2f& q : out[k].pts) {
                    mnx = std::min(mnx, q.x); mxx = std::max(mxx, q.x);
                    mny = std::min(mny, q.y); mxy = std::max(mxy, q.y);
                }
                const cv::Rect2f hr(mnx, mny, mxx - mnx, mxy - mny);
                if (BoxIoU(pr, hr) > 0.1f || hr.contains(cv::Point2f((p.x1 + p.x2) / 2, (p.y1 + p.y2) / 2))) dup = true;
            }
            if (!dup && estimate(bgr, p, out[n])) ++n;
        }
    }
    nLast_ = n;
    for (int k = 0; k < n; ++k) last_[k] = out[k];
    return n;
}

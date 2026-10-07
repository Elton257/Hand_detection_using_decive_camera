#include "Gestures.hpp"
#include "Config.hpp"

#include <cmath>

float GestureClassifier::dist(const cv::Point2f& a, const cv::Point2f& b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Angle (degrees) at point b between a and c.
float GestureClassifier::angleAt(const cv::Point2f& a, const cv::Point2f& b, const cv::Point2f& c) {
    const float ux = a.x - b.x, uy = a.y - b.y, vx = c.x - b.x, vy = c.y - b.y;
    const float lu = std::sqrt(ux * ux + uy * uy), lv = std::sqrt(vx * vx + vy * vy);
    if (lu < 1e-3f || lv < 1e-3f) return 0.f;
    float cs = (ux * vx + uy * vy) / (lu * lv);
    cs = cs < -1.f ? -1.f : (cs > 1.f ? 1.f : cs);
    return std::acos(cs) * 57.2957795f;
}

// Extended finger: straight at the middle joint (PIP) and the tip farther from the wrist than the joint.
bool GestureClassifier::extended(const HandResult& h, int mcp, int pip, int tip) {
    const cv::Point2f& w = h.pts[WRIST];
    return angleAt(h.pts[mcp], h.pts[pip], h.pts[tip]) > cfg::FINGER_STRAIGHT_DEG &&
           dist(h.pts[tip], w) > dist(h.pts[pip], w) * 1.05f;
}

FingerState GestureClassifier::readFingers(const HandResult& h) {
    FingerState s;
    const float palm = dist(h.pts[WRIST], h.pts[MIDDLE_MCP]);   // palm length (scale)
    s.thumb = palm > 1e-3f &&
              dist(h.pts[THUMB_TIP], h.pts[INDEX_MCP]) > cfg::THUMB_OUT_RATIO * palm &&
              angleAt(h.pts[THUMB_MCP], h.pts[THUMB_IP], h.pts[THUMB_TIP]) > 130.f;
    s.index  = extended(h, INDEX_MCP, INDEX_PIP, INDEX_TIP);
    s.middle = extended(h, MIDDLE_MCP, MIDDLE_PIP, MIDDLE_TIP);
    s.ring   = extended(h, RING_MCP, RING_PIP, RING_TIP);
    s.pinky  = extended(h, PINKY_MCP, PINKY_PIP, PINKY_TIP);
    return s;
}

RState GestureClassifier::right(const HandResult& h, float& tx, float& ty) {
    const FingerState f = readFingers(h);
    if (f.index && !f.middle && !f.ring && !f.pinky) {
        tx = h.pts[INDEX_TIP].x;
        ty = h.pts[INDEX_TIP].y;
        return f.thumb ? RState::Draw : RState::Point;
    }
    if (f.index && f.middle && !f.ring && !f.pinky) {
        tx = (h.pts[INDEX_TIP].x + h.pts[MIDDLE_TIP].x) * 0.5f;
        ty = (h.pts[INDEX_TIP].y + h.pts[MIDDLE_TIP].y) * 0.5f;
        return RState::Erase;
    }
    return RState::Idle;
}

LState GestureClassifier::left(const HandResult& h) {
    const FingerState f = readFingers(h);
    if (f.index && !f.middle && !f.ring && !f.pinky) return LState::ZoomIn;
    if (f.index && f.middle && !f.ring && !f.pinky) return LState::ZoomOut;
    return LState::None;
}

static float Alpha(float cutoff, float dt) {
    const float tau = 1.0f / (6.28318530718f * cutoff);
    return 1.0f / (1.0f + tau / dt);
}

float OneEuro::filter(float x, float dt) {
    if (!has_) { has_ = true; y_ = x; dy_ = 0; last_ = x; return x; }
    const float d = (x - last_) / dt;
    last_ = x;
    dy_ += Alpha(cfg::EURO_DCUT, dt) * (d - dy_);
    const float cut = cfg::EURO_MINCUT + cfg::EURO_BETA * std::fabs(dy_);
    y_ += Alpha(cut, dt) * (x - y_);
    return y_;
}

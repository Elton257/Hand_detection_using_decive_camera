// ============================================================================
//  Gestures.hpp - from the 21 hand landmarks to an action + cursor smoothing
// ============================================================================
#pragma once
#include "HandDetector.hpp"

enum class RState { None, Idle, Point, Draw, Erase };   // right hand
enum class LState { None, ZoomIn, ZoomOut };            // left hand

// A gesture is accepted only after it is seen for 'need' consecutive frames (no flicker).
template <class E>
struct Debounce {
    E stable, cand;
    int count = 0;
    explicit Debounce(E s) : stable(s), cand(s) {}
    E update(E s, int need) {
        if (s == stable) { cand = s; count = 0; return stable; }
        if (s == cand) ++count; else { cand = s; count = 1; }
        if (count >= need) { stable = s; count = 0; }
        return stable;
    }
};

struct FingerState { bool thumb, index, middle, ring, pinky; };

// Turns the 21 landmarks of one hand into a gesture. Stateless (all static).
class GestureClassifier {
public:
    // Which fingers are extended (independent of hand rotation).
    static FingerState readFingers(const HandResult& h);

    // Right hand: state and cursor point (tx,ty) in image pixels.
    //   index only              -> Point (move)
    //   index + thumb           -> Draw  (draw)
    //   index + middle (V)      -> Erase (erase)
    static RState right(const HandResult& h, float& tx, float& ty);

    // Left hand: 1 finger (index) -> ZoomIn, 2 fingers (V) -> ZoomOut.
    static LState left(const HandResult& h);

private:
    static float dist(const cv::Point2f& a, const cv::Point2f& b);
    static float angleAt(const cv::Point2f& a, const cv::Point2f& b, const cv::Point2f& c);
    static bool extended(const HandResult& h, int mcp, int pip, int tip);
};

// One-Euro filter: steady cursor when the hand rests, no lag when it moves fast.
class OneEuro {
public:
    void reset() { has_ = false; }
    float filter(float x, float dt);
private:
    float y_ = 0, dy_ = 0, last_ = 0;
    bool has_ = false;
};

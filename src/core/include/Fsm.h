#pragma once

#include "Inference.h"
#include "Config.h"
#include <chrono>
#include <string>
#include <vector>

namespace gesture {

enum class State {
    HOVER,
    PINCH,
    FIST,
    SWIPE,
    ZOOM,
    POINT
};

// Bitmask directions for swipes
enum SwipeDirection {
    SWIPE_NONE = 0,
    SWIPE_LEFT = 1,
    SWIPE_RIGHT = 2,
    SWIPE_UP = 4,
    SWIPE_DOWN = 8
};

struct GestureOutput {
    State state = State::HOVER;
    float cursorX = 0.5f;
    float cursorY = 0.5f;
    uint32_t swipeDirection = SWIPE_NONE;
    bool pinchStarted = false;
    bool pinchReleased = false;
};

class Fsm {
public:
    Fsm();
    ~Fsm();

    // Run FSM update pass using current configuration parameters
    void Update(const std::vector<Landmark>& landmarks, const Config& config);
    
    // Retrieve the current gesture FSM state and detailed outputs
    const GestureOutput& GetOutput() const { return m_output; }
    std::string StateToString(State state) const;

private:
    State EvaluateRawState(const std::vector<Landmark>& landmarks, const Config& config);
    void UpdateSwipeTracker(const std::vector<Landmark>& landmarks, const Config& config);

    GestureOutput m_output;
    State m_pendingState;
    std::chrono::steady_clock::time_point m_pendingStateStartTime;
    
    // Swipe velocity tracking structures
    struct Centroid {
        float x;
        float y;
        std::chrono::steady_clock::time_point time;
    };
    std::vector<Centroid> m_centroidHistory;
};

} // namespace gesture

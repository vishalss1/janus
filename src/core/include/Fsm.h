#pragma once

#include "Inference.h"
#include <chrono>
#include <string>

namespace gesture {

enum class State {
    HOVER,
    PINCH,
    FIST,
    SWIPE,
    ZOOM,
    POINT
};

class Fsm {
public:
    Fsm();
    ~Fsm();

    void Update(const std::vector<Landmark>& landmarks);
    State GetCurrentState() const { return m_currentState; }
    std::string StateToString(State state) const;

private:
    State EvaluateRawState(const std::vector<Landmark>& landmarks);

    State m_currentState;
    State m_pendingState;
    std::chrono::steady_clock::time_point m_pendingStateStartTime;
    
    // Configurable debounce duration
    std::chrono::milliseconds m_debounceThreshold;
};

} // namespace gesture

#include "Fsm.h"
#include <cmath>
#include <iostream>

namespace gesture {

Fsm::Fsm() 
    : m_currentState(State::HOVER)
    , m_pendingState(State::HOVER)
    , m_debounceThreshold(150) // 150 milliseconds
{
    m_pendingStateStartTime = std::chrono::steady_clock::now();
}

Fsm::~Fsm() {}

std::string Fsm::StateToString(State state) const {
    switch (state) {
        case State::HOVER: return "HOVER";
        case State::PINCH: return "PINCH";
        case State::FIST:  return "FIST";
        case State::SWIPE: return "SWIPE";
        case State::ZOOM:  return "ZOOM";
        case State::POINT: return "POINT";
        default:           return "UNKNOWN";
    }
}

State Fsm::EvaluateRawState(const std::vector<Landmark>& landmarks) {
    if (landmarks.size() < 21) return State::HOVER;

    // Extract thumb and index tip positions
    const auto& thumb = landmarks[4];
    const auto& index = landmarks[8];

    // Calculate 3D distance
    float dx = thumb.x - index.x;
    float dy = thumb.y - index.y;
    float dz = thumb.z - index.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    // Simple heuristic threshold for pinch vs hover
    if (dist < 0.05f) {
        return State::PINCH;
    }

    return State::HOVER;
}

void Fsm::Update(const std::vector<Landmark>& landmarks) {
    State rawState = EvaluateRawState(landmarks);

    if (rawState == m_currentState) {
        // Raw state matches current state, reset transition tracking
        m_pendingState = m_currentState;
        return;
    }

    if (rawState != m_pendingState) {
        // Raw state changed to a new pending state, start timer
        m_pendingState = rawState;
        m_pendingStateStartTime = std::chrono::steady_clock::now();
    } else {
        // Raw state matches our pending state, check if debounce time has passed
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_pendingStateStartTime);
        
        if (elapsed >= m_debounceThreshold) {
            std::cout << "State transition: " << StateToString(m_currentState) 
                      << " -> " << StateToString(m_pendingState) << std::endl;
            m_currentState = m_pendingState;
        }
    }
}

} // namespace gesture

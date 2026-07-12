#include "Fsm.h"
#include <cmath>
#include <iostream>
#include <algorithm>

namespace gesture {

Fsm::Fsm() 
    : m_pendingState(State::HOVER)
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

static float GetDistance(const Landmark& a, const Landmark& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

State Fsm::EvaluateRawState(const std::vector<Landmark>& landmarks, const Config& config) {
    if (landmarks.size() < 21) return State::HOVER;

    // Define landmarks references
    const auto& wrist = landmarks[0];
    const auto& thumb_tip = landmarks[4];
    const auto& index_tip = landmarks[8];
    const auto& index_mcp = landmarks[5];
    const auto& middle_tip = landmarks[12];
    const auto& middle_mcp = landmarks[9];
    const auto& ring_tip = landmarks[16];
    const auto& ring_mcp = landmarks[13];
    const auto& pinky_tip = landmarks[20];
    const auto& pinky_mcp = landmarks[17];

    // Hand size scale reference (wrist to middle finger MCP)
    float handScale = GetDistance(wrist, middle_mcp);
    if (handScale < 0.001f) handScale = 1.0f;

    // Calculate normalized distances from wrist
    float distThumb = GetDistance(wrist, thumb_tip) / handScale;
    float distIndex = GetDistance(wrist, index_tip) / handScale;
    float distMiddle = GetDistance(wrist, middle_tip) / handScale;
    float distRing = GetDistance(wrist, ring_tip) / handScale;
    float distPinky = GetDistance(wrist, pinky_tip) / handScale;

    // A finger is considered extended if it is far from the wrist
    bool thumbExtended = distThumb > 1.2f;
    bool indexExtended = distIndex > 1.3f;
    bool middleExtended = distMiddle > 1.3f;
    bool ringExtended = distRing > 1.3f;
    bool pinkyExtended = distPinky > 1.3f;

    // 1. Evaluate Fist Gesture
    if (!indexExtended && !middleExtended && !ringExtended && !pinkyExtended) {
        return State::FIST;
    }

    // 2. Evaluate Pinch Gesture (Thumb + Index tips close together)
    float pinchDist = GetDistance(thumb_tip, index_tip);
    
    // Hysteresis for pinch
    float currentPinchThreshold = (m_output.state == State::PINCH) ? config.pinchExitDist : config.pinchEnterDist;
    if (pinchDist < currentPinchThreshold) {
        return State::PINCH;
    }

    // 3. Evaluate Zoom Gesture (Thumb + Middle finger tips close together while others extended)
    float zoomDist = GetDistance(thumb_tip, middle_tip);
    if (zoomDist < currentPinchThreshold && indexExtended && ringExtended) {
        return State::ZOOM;
    }

    // 4. Evaluate Point Gesture (Only Index extended)
    if (indexExtended && !middleExtended && !ringExtended && !pinkyExtended) {
        return State::POINT;
    }

    // Default to Hover
    return State::HOVER;
}

void Fsm::UpdateSwipeTracker(const std::vector<Landmark>& landmarks, const Config& config) {
    if (landmarks.empty()) {
        m_centroidHistory.clear();
        m_output.swipeDirection = SWIPE_NONE;
        return;
    }

    // Centroid of the palm
    float cx = 0, cy = 0;
    int count = 0;
    // Landmarks 0, 5, 9, 13, 17 form the base palm structure
    const int palmIndices[] = {0, 5, 9, 13, 17};
    for (int idx : palmIndices) {
        cx += landmarks[idx].x;
        cy += landmarks[idx].y;
        count++;
    }
    cx /= count;
    cy /= count;

    Centroid currentCentroid = { cx, cy, std::chrono::steady_clock::now() };
    m_centroidHistory.push_back(currentCentroid);

    if (m_centroidHistory.size() > config.swipeWindowFrames) {
        m_centroidHistory.erase(m_centroidHistory.begin());
    }

    m_output.swipeDirection = SWIPE_NONE;

    if (m_centroidHistory.size() >= 3) {
        const auto& oldest = m_centroidHistory.front();
        const auto& newest = m_centroidHistory.back();

        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(newest.time - oldest.time).count();
        if (dt > 0) {
            float dx = newest.x - oldest.x;
            float dy = newest.y - oldest.y;
            
            // Calculate velocity per millisecond
            float vx = dx / dt;
            float vy = dy / dt;
            
            // Convert threshold from "velocity per frame" to "velocity per millisecond"
            // Assumed target of 33ms per frame: threshold_ms = threshold / 33
            float velocityThresholdMs = config.swipeVelocityThreshold / 33.0f;
            
            float speedX = std::abs(vx);
            float speedY = std::abs(vy);

            if (speedX > velocityThresholdMs || speedY > velocityThresholdMs) {
                if (speedX > speedY) {
                    m_output.swipeDirection = (vx > 0) ? SWIPE_RIGHT : SWIPE_LEFT;
                } else {
                    m_output.swipeDirection = (vy > 0) ? SWIPE_DOWN : SWIPE_UP;
                }
            }
        }
    }
}

void Fsm::Update(const std::vector<Landmark>& landmarks, const Config& config) {
    State rawState = EvaluateRawState(landmarks, config);
    UpdateSwipeTracker(landmarks, config);

    State previousState = m_output.state;

    // Debounce implementation
    if (rawState == m_output.state) {
        m_pendingState = m_output.state;
    } else {
        if (rawState != m_pendingState) {
            m_pendingState = rawState;
            m_pendingStateStartTime = std::chrono::steady_clock::now();
        } else {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_pendingStateStartTime);
            
            if (elapsed.count() >= static_cast<long long>(config.debounceMs)) {
                m_output.state = m_pendingState;
            }
        }
    }

    // Set gesture-specific flags
    m_output.pinchStarted = (m_output.state == State::PINCH && previousState != State::PINCH);
    m_output.pinchReleased = (m_output.state != State::PINCH && previousState == State::PINCH);

    // If swipe velocity is high and palm is open, override state to SWIPE
    if (m_output.swipeDirection != SWIPE_NONE && m_output.state == State::HOVER) {
        m_output.state = State::SWIPE;
    }

    // Set cursor position tracking (index tip, landmark 8)
    if (!landmarks.empty()) {
        m_output.cursorX = landmarks[8].x;
        m_output.cursorY = landmarks[8].y;
    }
}

} // namespace gesture

#include "InputInjector.h"
#include <iostream>
#include <algorithm>

namespace gesture {

InputInjector::InputInjector() 
    : m_lastX(0.5f)
    , m_lastY(0.5f)
    , m_firstUpdate(true)
{}

InputInjector::~InputInjector() {}

void InputInjector::SendTrackingUpdate(const GestureOutput& output) {
    if (m_firstUpdate) {
        m_lastX = output.cursorX;
        m_lastY = output.cursorY;
        m_firstUpdate = false;
        return;
    }

    // Calculate relative differences
    float dx = (output.cursorX - m_lastX);
    float dy = (output.cursorY - m_lastY);

    // Dynamic mouse sensitivity scalar (convert 0-1 coordinate diff to relative pixels)
    constexpr float SENSITIVITY = 2500.0f;
    float pixelDx = dx * SENSITIVITY;
    float pixelDy = dy * SENSITIVITY;

    // Send mouse move relative event
    INPUT inputMove = {0};
    inputMove.type = INPUT_MOUSE;
    inputMove.mi.dwFlags = MOUSEEVENTF_MOVE;
    inputMove.mi.dx = static_cast<LONG>(pixelDx);
    inputMove.mi.dy = static_cast<LONG>(pixelDy);
    SendInput(1, &inputMove, sizeof(INPUT));

    // Handle clicks/drags via pinch transition events (using FSM-tracked started/released flags)
    if (output.pinchStarted) {
        INPUT inputClick = {0};
        inputClick.type = INPUT_MOUSE;
        inputClick.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &inputClick, sizeof(INPUT));
    }
    if (output.pinchReleased) {
        INPUT inputClick = {0};
        inputClick.type = INPUT_MOUSE;
        inputClick.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &inputClick, sizeof(INPUT));
    }

    // Handle scroll wheel via SWIPE gestures
    if (output.swipeDirection == SWIPE_UP) {
        INPUT inputScroll = {0};
        inputScroll.type = INPUT_MOUSE;
        inputScroll.mi.dwFlags = MOUSEEVENTF_WHEEL;
        inputScroll.mi.mouseData = WHEEL_DELTA;
        SendInput(1, &inputScroll, sizeof(INPUT));
    } else if (output.swipeDirection == SWIPE_DOWN) {
        INPUT inputScroll = {0};
        inputScroll.type = INPUT_MOUSE;
        inputScroll.mi.dwFlags = MOUSEEVENTF_WHEEL;
        inputScroll.mi.mouseData = -WHEEL_DELTA;
        SendInput(1, &inputScroll, sizeof(INPUT));
    }

    // Update tracking states
    m_lastX = output.cursorX;
    m_lastY = output.cursorY;
}

} // namespace gesture
